#pragma once
// Google sign-in for a desktop app: OAuth 2.0 authorization code flow with PKCE and a
// loopback redirect (the user signs in in their normal browser).
// Scope: drive.file (only files this app creates) + email (to show who is signed in).

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;

class GoogleAuth : public QObject {
    Q_OBJECT
public:
    using TokenCallback = std::function<void(const QString& accessToken, const QString& error)>;

    explicit GoogleAuth(QNetworkAccessManager* nam, QObject* parent = nullptr);

    // False if this build was compiled without a Google OAuth client ID.
    static bool isConfigured();

    bool isSignedIn() const;
    QString email() const;

    // Opens the browser for the user to sign in. Emits signInFinished.
    void signIn();
    void cancelSignIn();
    bool signInInProgress() const { return m_server != nullptr; }
    void signOut();

    // Gets a valid access token (refreshing it if needed) and passes it to `cb`.
    void withAccessToken(const TokenCallback& cb);
    // Forget the cached access token (e.g. after a 401) so the next call refreshes it.
    void invalidateAccessToken();

signals:
    void signInFinished(bool ok, const QString& error);
    void signedInChanged();

private:
    void handleRedirect(const QString& requestTarget);
    void exchangeCode(const QString& code);
    void fetchEmail();
    void finishSignIn(bool ok, const QString& error);
    void refresh();
    void stopServer();

    QNetworkAccessManager* m_nam;
    QTcpServer* m_server = nullptr;
    QTimer* m_timeout = nullptr;
    QString m_verifier, m_state, m_redirectUri;

    QString m_accessToken;
    QDateTime m_expiry;
    bool m_refreshing = false;
    QList<TokenCallback> m_waiting;
};
