#include "GoogleAuth.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#ifndef GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_ID ""
#endif
#ifndef GOOGLE_CLIENT_SECRET
#define GOOGLE_CLIENT_SECRET ""
#endif

namespace {
const QString kAuthUrl = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
const QString kTokenUrl = QStringLiteral("https://oauth2.googleapis.com/token");
const QString kRevokeUrl = QStringLiteral("https://oauth2.googleapis.com/revoke");
const QString kUserInfoUrl = QStringLiteral("https://openidconnect.googleapis.com/v1/userinfo");
const QString kScopes = QStringLiteral("openid email https://www.googleapis.com/auth/drive.file");

const QString kRefreshKey = QStringLiteral("google/refreshToken");
const QString kEmailKey = QStringLiteral("google/email");

QString clientId() { return QStringLiteral(GOOGLE_CLIENT_ID); }
// Google issues a "secret" for desktop clients, but it can't be kept secret in a distributed
// app; PKCE is what protects the sign-in. Google documents this as expected for installed apps.
QString clientSecret() { return QStringLiteral(GOOGLE_CLIENT_SECRET); }

QString randomUrlSafe(int bytes)
{
    QByteArray raw(bytes, Qt::Uninitialized);
    for (char& c : raw)
        c = char(QRandomGenerator::system()->bounded(256));
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// application/x-www-form-urlencoded body with every reserved character escaped.
QByteArray encodeForm(const QUrlQuery& q)
{
    QByteArray out;
    for (const auto& [key, value] : q.queryItems(QUrl::FullyDecoded)) {
        if (!out.isEmpty())
            out += '&';
        out += QUrl::toPercentEncoding(key) + '=' + QUrl::toPercentEncoding(value);
    }
    return out;
}

QNetworkRequest formRequest(const QString& url)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(30000);
    return req;
}

// Error text from a Google OAuth/API JSON error reply.
QString errorText(QNetworkReply* reply, const QJsonObject& body)
{
    const QString desc = body.value(QStringLiteral("error_description")).toString();
    if (!desc.isEmpty())
        return desc;
    const QJsonValue err = body.value(QStringLiteral("error"));
    if (err.isString())
        return err.toString();
    if (err.isObject())
        return err.toObject().value(QStringLiteral("message")).toString();
    return reply->errorString();
}

const char* kBrowserPage =
    "<!doctype html><html><head><meta charset='utf-8'><title>Pilot Logbook</title>"
    "<style>body{font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;"
    "height:90vh;margin:0;background:#f3f4f6;color:#1b1f24}div{background:#fff;padding:32px 40px;"
    "border-radius:12px;box-shadow:0 4px 20px rgba(0,0,0,.08);text-align:center}</style></head>"
    "<body><div><h2>%1</h2><p>%2</p></div></body></html>";
}  // namespace

GoogleAuth::GoogleAuth(QNetworkAccessManager* nam, QObject* parent) : QObject(parent), m_nam(nam) {}

bool GoogleAuth::isConfigured()
{
    return !clientId().isEmpty();
}

bool GoogleAuth::isSignedIn() const
{
    return !QSettings().value(kRefreshKey).toString().isEmpty();
}

QString GoogleAuth::email() const
{
    return QSettings().value(kEmailKey).toString();
}

void GoogleAuth::signIn()
{
    if (!isConfigured()) {
        emit signInFinished(false, tr("Google backup isn't set up in this build of the app."));
        return;
    }
    stopServer();
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        const QString err = m_server->errorString();
        stopServer();
        emit signInFinished(false, tr("Couldn't start sign-in: %1").arg(err));
        return;
    }
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
    m_verifier = randomUrlSafe(48);
    m_state = randomUrlSafe(16);
    const QString challenge = QString::fromLatin1(
        QCryptographicHash::hash(m_verifier.toLatin1(), QCryptographicHash::Sha256)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (m_server && m_server->hasPendingConnections()) {
            QTcpSocket* sock = m_server->nextPendingConnection();
            connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
            connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
                if (sock->property("handled").toBool())
                    return;
                const QByteArray data = sock->peek(8192);
                if (!data.contains("\r\n\r\n") && data.size() < 8192)
                    return;  // wait for the full request headers
                sock->readAll();
                sock->setProperty("handled", true);
                const QList<QByteArray> requestLine = data.left(data.indexOf("\r\n")).split(' ');
                const QString target = requestLine.size() >= 2 ? QString::fromLatin1(requestLine.at(1)) : QString();
                const bool isCallback = target.startsWith(QLatin1String("/?")) || target == QLatin1String("/");
                const bool ok = isCallback && target.contains(QLatin1String("code="));
                const QString title = ok ? tr("You're signed in") : tr("Sign-in didn't complete");
                const QString body = ok ? tr("You can close this tab and return to Pilot Logbook.")
                                        : tr("Return to Pilot Logbook and try again.");
                QByteArray page = QString::fromLatin1(kBrowserPage).arg(title, body).toUtf8();
                QByteArray resp = (isCallback ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n");
                resp += "Content-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                        + QByteArray::number(page.size()) + "\r\n\r\n" + page;
                sock->write(resp);
                sock->disconnectFromHost();
                if (isCallback)
                    handleRedirect(target);
            });
        }
    });

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { finishSignIn(false, tr("Sign-in timed out.")); });
    m_timeout->start(5 * 60 * 1000);

    QUrl url(kAuthUrl);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), clientId());
    q.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("scope"), kScopes);
    q.addQueryItem(QStringLiteral("code_challenge"), challenge);
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("state"), m_state);
    q.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    q.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent select_account"));
    url.setQuery(q);
    if (!QDesktopServices::openUrl(url))
        finishSignIn(false, tr("Couldn't open your web browser."));
}

void GoogleAuth::cancelSignIn()
{
    if (m_server)
        finishSignIn(false, tr("Sign-in cancelled."));
}

void GoogleAuth::handleRedirect(const QString& requestTarget)
{
    if (m_state.isEmpty() || !m_server)
        return;  // already handled
    const QUrlQuery q(QUrl(requestTarget).query());
    if (q.queryItemValue(QStringLiteral("state")) != m_state) {
        finishSignIn(false, tr("Sign-in failed a security check. Please try again."));
        return;
    }
    const QString error = q.queryItemValue(QStringLiteral("error"));
    if (!error.isEmpty()) {
        finishSignIn(false, error == QLatin1String("access_denied") ? tr("Access wasn't granted.")
                                                                    : tr("Google reported: %1").arg(error));
        return;
    }
    const QString code = q.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
    stopServer();  // only one redirect is expected
    exchangeCode(code);
}

void GoogleAuth::exchangeCode(const QString& code)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("client_secret"), clientSecret());
    form.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
    QNetworkReply* reply = m_nam->post(formRequest(kTokenUrl), encodeForm(form));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        const QString refreshToken = body.value(QStringLiteral("refresh_token")).toString();
        if (reply->error() != QNetworkReply::NoError || refreshToken.isEmpty()) {
            finishSignIn(false, tr("Couldn't complete sign-in: %1").arg(errorText(reply, body)));
            return;
        }
        m_accessToken = body.value(QStringLiteral("access_token")).toString();
        m_expiry = QDateTime::currentDateTimeUtc().addSecs(body.value(QStringLiteral("expires_in")).toInt(3600) - 60);
        QSettings().setValue(kRefreshKey, refreshToken);
        fetchEmail();
    });
}

void GoogleAuth::fetchEmail()
{
    QNetworkRequest req{QUrl(kUserInfoUrl)};
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
    req.setTransferTimeout(30000);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        QSettings().setValue(kEmailKey, body.value(QStringLiteral("email")).toString());
        finishSignIn(true, {});
    });
}

void GoogleAuth::finishSignIn(bool ok, const QString& error)
{
    stopServer();
    m_verifier.clear();
    m_state.clear();
    emit signInFinished(ok, error);
    if (ok)
        emit signedInChanged();
}

void GoogleAuth::stopServer()
{
    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void GoogleAuth::signOut()
{
    QSettings s;
    const QString token = s.value(kRefreshKey).toString();
    if (!token.isEmpty()) {
        // Best effort: tell Google to revoke this app's access.
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("token"), token);
        QNetworkReply* reply = m_nam->post(formRequest(kRevokeUrl), encodeForm(form));
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    s.remove(kRefreshKey);
    s.remove(kEmailKey);
    m_accessToken.clear();
    emit signedInChanged();
}

void GoogleAuth::invalidateAccessToken()
{
    m_accessToken.clear();
}

void GoogleAuth::withAccessToken(const TokenCallback& cb)
{
    if (!isSignedIn()) {
        cb({}, tr("Not connected to a Google account."));
        return;
    }
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiry) {
        cb(m_accessToken, {});
        return;
    }
    m_waiting << cb;
    if (!m_refreshing)
        refresh();
}

void GoogleAuth::refresh()
{
    m_refreshing = true;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("client_secret"), clientSecret());
    form.addQueryItem(QStringLiteral("refresh_token"), QSettings().value(kRefreshKey).toString());
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    QNetworkReply* reply = m_nam->post(formRequest(kTokenUrl), encodeForm(form));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_refreshing = false;
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        QString error;
        if (reply->error() == QNetworkReply::NoError && body.contains(QStringLiteral("access_token"))) {
            m_accessToken = body.value(QStringLiteral("access_token")).toString();
            m_expiry = QDateTime::currentDateTimeUtc().addSecs(body.value(QStringLiteral("expires_in")).toInt(3600) - 60);
        } else if (body.value(QStringLiteral("error")).toString() == QLatin1String("invalid_grant")) {
            // Access was revoked or expired: the user needs to connect again.
            QSettings().remove(kRefreshKey);
            error = tr("Google access has expired or was removed. Connect your Google account again.");
            emit signedInChanged();
        } else {
            error = tr("Couldn't reach Google: %1").arg(errorText(reply, body));
        }
        const QList<TokenCallback> waiting = std::exchange(m_waiting, {});
        for (const TokenCallback& cb : waiting)
            cb(error.isEmpty() ? m_accessToken : QString(), error);
    });
}
