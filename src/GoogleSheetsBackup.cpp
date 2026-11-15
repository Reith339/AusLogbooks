#include "GoogleSheetsBackup.h"

#include "Columns.h"
#include "Database.h"
#include "GoogleAuth.h"
#include "SheetFormat.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>

namespace {
const QString kSheetIdKey = QStringLiteral("google/spreadsheetId");
const QString kLastBackupKey = QStringLiteral("google/lastBackup");
const QString kSheetsApi = QStringLiteral("https://sheets.googleapis.com/v4/spreadsheets");
const QString kDriveFilesApi = QStringLiteral("https://www.googleapis.com/drive/v3/files");
constexpr int kDebounceMs = 4000;
constexpr int kRetryMs = 5 * 60 * 1000;

// 'Sheet Name'!A1 notation.
QString range(const QString& sheet, const QString& cells = QString())
{
    QString r = QLatin1Char('\'') + QString(sheet).replace(QLatin1Char('\''), QStringLiteral("''")) + QLatin1Char('\'');
    return cells.isEmpty() ? r : r + QLatin1Char('!') + cells;
}

QJsonObject sheetProps(const QString& title, int index)
{
    return QJsonObject{{QStringLiteral("properties"),
                        QJsonObject{{QStringLiteral("title"), title},
                                    {QStringLiteral("index"), index},
                                    {QStringLiteral("gridProperties"),
                                     QJsonObject{{QStringLiteral("frozenRowCount"), 1}}}}}};
}

QJsonObject boldHeader(int sheetId)
{
    return QJsonObject{{QStringLiteral("repeatCell"),
                        QJsonObject{{QStringLiteral("range"),
                                     QJsonObject{{QStringLiteral("sheetId"), sheetId},
                                                 {QStringLiteral("startRowIndex"), 0},
                                                 {QStringLiteral("endRowIndex"), 1}}},
                                    {QStringLiteral("cell"),
                                     QJsonObject{{QStringLiteral("userEnteredFormat"),
                                                  QJsonObject{{QStringLiteral("textFormat"),
                                                               QJsonObject{{QStringLiteral("bold"), true}}}}}}},
                                    {QStringLiteral("fields"), QStringLiteral("userEnteredFormat.textFormat.bold")}}}};
}

QJsonObject hoursFormat(int sheetId, int column)
{
    return QJsonObject{
        {QStringLiteral("repeatCell"),
         QJsonObject{{QStringLiteral("range"),
                      QJsonObject{{QStringLiteral("sheetId"), sheetId},
                                  {QStringLiteral("startRowIndex"), 1},
                                  {QStringLiteral("startColumnIndex"), column},
                                  {QStringLiteral("endColumnIndex"), column + 1}}},
                     {QStringLiteral("cell"),
                      QJsonObject{{QStringLiteral("userEnteredFormat"),
                                   QJsonObject{{QStringLiteral("numberFormat"),
                                                QJsonObject{{QStringLiteral("type"), QStringLiteral("NUMBER")},
                                                            {QStringLiteral("pattern"), QStringLiteral("0.0")}}}}}}},
                     {QStringLiteral("fields"), QStringLiteral("userEnteredFormat.numberFormat")}}}};
}
}  // namespace

GoogleSheetsBackup::GoogleSheetsBackup(Database& db, GoogleAuth* auth, QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_db(db), m_auth(auth), m_nam(nam)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &GoogleSheetsBackup::runBackup);
    m_retry.setSingleShot(true);
    m_retry.setInterval(kRetryMs);
    connect(&m_retry, &QTimer::timeout, this, &GoogleSheetsBackup::runBackup);
    m_lastSuccess = QSettings().value(kLastBackupKey).toDateTime();
    connect(m_auth, &GoogleAuth::signedInChanged, this, &GoogleSheetsBackup::statusChanged);
}

QString GoogleSheetsBackup::spreadsheetId() const
{
    return QSettings().value(kSheetIdKey).toString();
}

QUrl GoogleSheetsBackup::spreadsheetUrl() const
{
    const QString id = spreadsheetId();
    return id.isEmpty() ? QUrl() : QUrl(QStringLiteral("https://docs.google.com/spreadsheets/d/%1/edit").arg(id));
}

void GoogleSheetsBackup::setSpreadsheetId(const QString& id)
{
    QSettings().setValue(kSheetIdKey, id);
    emit statusChanged();
}

QString GoogleSheetsBackup::statusText() const
{
    if (!m_auth->isSignedIn())
        return tr("Google Sheets backup: off");
    if (m_busy)
        return tr("Google Sheets: backing up…");
    if (!m_lastError.isEmpty())
        return tr("Google Sheets: backup failed — will retry");
    if (m_debounce.isActive())
        return tr("Google Sheets: changes waiting to back up");
    if (m_lastSuccess.isValid()) {
        const QDateTime local = m_lastSuccess.toLocalTime();
        const QString when = local.date() == QDate::currentDate()
                                 ? local.toString(QStringLiteral("HH:mm"))
                                 : local.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        return tr("Google Sheets: backed up %1").arg(when);
    }
    return tr("Google Sheets: connected");
}

void GoogleSheetsBackup::scheduleBackup()
{
    if (!m_auth->isSignedIn())
        return;
    if (m_busy)
        m_dirty = true;
    else
        m_debounce.start();
    emit statusChanged();
}

void GoogleSheetsBackup::backupNow()
{
    m_debounce.stop();
    if (m_busy)
        m_dirty = true;
    else
        runBackup();
}

void GoogleSheetsBackup::flush(int msTimeout)
{
    if (!m_auth->isSignedIn())
        return;
    if (m_debounce.isActive() || m_dirty)
        backupNow();
    if (!m_busy)
        return;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &GoogleSheetsBackup::statusChanged, &loop, [this, &loop] {
        if (!m_busy)
            loop.quit();
    });
    guard.start(msTimeout);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
void GoogleSheetsBackup::api(const QByteArray& verb, const QUrl& url, const QJsonObject& body,
                             const JsonCallback& cb, bool retried)
{
    m_auth->withAccessToken([=, this](const QString& token, const QString& authError) {
        if (!authError.isEmpty()) {
            cb({}, 0, authError);
            return;
        }
        QNetworkRequest req(url);
        req.setRawHeader("Authorization", "Bearer " + token.toLatin1());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setTransferTimeout(60000);
        const QByteArray payload = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);
        QNetworkReply* reply = m_nam->sendCustomRequest(req, verb, payload);
        connect(reply, &QNetworkReply::finished, this, [=, this] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            if (status == 401 && !retried) {
                m_auth->invalidateAccessToken();
                api(verb, url, body, cb, true);
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                QString msg = json.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
                if (msg.isEmpty())
                    msg = reply->errorString();
                cb(json, status, msg);
                return;
            }
            cb(json, status, {});
        });
    });
}

// ---------------------------------------------------------------------------
// Backup
// ---------------------------------------------------------------------------
void GoogleSheetsBackup::runBackup()
{
    m_retry.stop();
    if (!m_auth->isSignedIn() || m_busy)
        return;
    m_busy = true;
    m_dirty = false;
    emit statusChanged();

    const QString id = spreadsheetId();
    if (!id.isEmpty()) {
        writeAll(id, true);
        return;
    }
    createSpreadsheet([this](const QString& newId, const QString& error) {
        if (!error.isEmpty()) {
            finishBackup(false, error);
            return;
        }
        setSpreadsheetId(newId);
        writeAll(newId, false);
    });
}

void GoogleSheetsBackup::createSpreadsheet(const std::function<void(const QString&, const QString&)>& cb)
{
    const QJsonObject body{
        {QStringLiteral("properties"), QJsonObject{{QStringLiteral("title"), SheetFormat::kSpreadsheetTitle}}},
        {QStringLiteral("sheets"),
         QJsonArray{sheetProps(SheetFormat::kFlights, 0), sheetProps(SheetFormat::kAircraft, 1),
                    sheetProps(SheetFormat::kBroughtForward, 2), sheetProps(SheetFormat::kSettings, 3)}}};
    api("POST", QUrl(kSheetsApi), body, [this, cb](const QJsonObject& json, int, const QString& error) {
        if (!error.isEmpty()) {
            cb({}, tr("Couldn't create the backup spreadsheet: %1").arg(error));
            return;
        }
        const QString id = json.value(QStringLiteral("spreadsheetId")).toString();
        // Formatting: bold headers, hours shown to one decimal place. Not critical if it fails.
        QHash<QString, int> sheetIds;
        for (const QJsonValue& s : json.value(QStringLiteral("sheets")).toArray()) {
            const QJsonObject p = s.toObject().value(QStringLiteral("properties")).toObject();
            sheetIds.insert(p.value(QStringLiteral("title")).toString(), p.value(QStringLiteral("sheetId")).toInt());
        }
        QJsonArray requests;
        for (int sid : sheetIds)
            requests.append(boldHeader(sid));
        for (int c : SheetFormat::flightHourColumns())
            requests.append(hoursFormat(sheetIds.value(SheetFormat::kFlights), c));
        requests.append(hoursFormat(sheetIds.value(SheetFormat::kBroughtForward), 2));
        api("POST", QUrl(kSheetsApi + QLatin1Char('/') + id + QStringLiteral(":batchUpdate")),
            QJsonObject{{QStringLiteral("requests"), requests}},
            [](const QJsonObject&, int, const QString&) {});
        cb(id, {});
    });
}

void GoogleSheetsBackup::writeAll(const QString& id, bool allowRecreate)
{
    QHash<qint64, Aircraft> byId;
    const QList<Aircraft> aircraft = m_db.aircraft();
    for (const Aircraft& a : aircraft)
        byId.insert(a.id, a);
    QMap<QString, QString> labels;
    for (const TimeColumn& c : timeColumns())
        if (c.renamable && columnLabel(c) != c.label)
            labels.insert(c.key, columnLabel(c));

    const QDateTime now = QDateTime::currentDateTime();
    const QList<QPair<QString, QJsonArray>> tabs = {
        {SheetFormat::kFlights, SheetFormat::flightRows(m_db.flights(), byId)},
        {SheetFormat::kAircraft, SheetFormat::aircraftRows(aircraft)},
        {SheetFormat::kBroughtForward, SheetFormat::broughtForwardRows(m_db.broughtForward())},
        {SheetFormat::kSettings, SheetFormat::settingsRows(labels, QApplication::applicationVersion(), now)},
    };

    // Write everything, then clear any leftover rows below it (so the sheet is never empty mid-way).
    QJsonArray data;
    QJsonArray clearRanges;
    for (const auto& [sheet, rows] : tabs) {
        data.append(QJsonObject{{QStringLiteral("range"), range(sheet, QStringLiteral("A1"))},
                                {QStringLiteral("majorDimension"), QStringLiteral("ROWS")},
                                {QStringLiteral("values"), rows}});
        clearRanges.append(range(sheet, QStringLiteral("A%1:ZZ").arg(rows.size() + 1)));
    }
    const QJsonObject body{{QStringLiteral("valueInputOption"), QStringLiteral("RAW")},
                           {QStringLiteral("data"), data}};
    const QString base = kSheetsApi + QLatin1Char('/') + id;
    api("POST", QUrl(base + QStringLiteral("/values:batchUpdate")), body,
        [=, this](const QJsonObject&, int status, const QString& error) {
            if ((status == 404 || status == 403) && allowRecreate) {
                // The spreadsheet was deleted (or access removed): start a new one.
                setSpreadsheetId(QString());
                m_busy = false;
                runBackup();
                return;
            }
            if (!error.isEmpty()) {
                finishBackup(false, error);
                return;
            }
            api("POST", QUrl(base + QStringLiteral("/values:batchClear")),
                QJsonObject{{QStringLiteral("ranges"), clearRanges}},
                [this](const QJsonObject&, int, const QString& clearError) { finishBackup(clearError.isEmpty(), clearError); });
        });
}

void GoogleSheetsBackup::finishBackup(bool ok, const QString& error)
{
    m_busy = false;
    if (ok) {
        m_lastError.clear();
        m_lastSuccess = QDateTime::currentDateTimeUtc();
        QSettings().setValue(kLastBackupKey, m_lastSuccess);
    } else {
        m_lastError = error;
        if (m_auth->isSignedIn())
            m_retry.start();
    }
    if (m_dirty && m_auth->isSignedIn())
        m_debounce.start();
    emit statusChanged();
}

// ---------------------------------------------------------------------------
// Find / restore
// ---------------------------------------------------------------------------
void GoogleSheetsBackup::findBackups(const FindCallback& cb)
{
    QUrl url(kDriveFilesApi);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"),
                   QStringLiteral("name = '%1' and mimeType = 'application/vnd.google-apps.spreadsheet' "
                                  "and trashed = false")
                       .arg(SheetFormat::kSpreadsheetTitle));
    q.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name,modifiedTime)"));
    q.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("modifiedTime desc"));
    q.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("20"));
    url.setQuery(q);
    api("GET", url, {}, [cb](const QJsonObject& json, int, const QString& error) {
        QList<SheetInfo> sheets;
        if (error.isEmpty()) {
            for (const QJsonValue& v : json.value(QStringLiteral("files")).toArray()) {
                const QJsonObject f = v.toObject();
                sheets << SheetInfo{f.value(QStringLiteral("id")).toString(), f.value(QStringLiteral("name")).toString(),
                                    QDateTime::fromString(f.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODate)};
            }
        }
        cb(sheets, error);
    });
}

void GoogleSheetsBackup::restoreFrom(const QString& id, const DoneCallback& cb)
{
    m_debounce.stop();
    QUrl url(kSheetsApi + QLatin1Char('/') + id + QStringLiteral("/values:batchGet"));
    QUrlQuery q;
    for (const QString& s : {SheetFormat::kFlights, SheetFormat::kAircraft, SheetFormat::kBroughtForward,
                             SheetFormat::kSettings})
        q.addQueryItem(QStringLiteral("ranges"), range(s));
    q.addQueryItem(QStringLiteral("valueRenderOption"), QStringLiteral("UNFORMATTED_VALUE"));
    q.addQueryItem(QStringLiteral("dateTimeRenderOption"), QStringLiteral("FORMATTED_STRING"));
    url.setQuery(q);
    api("GET", url, {}, [=, this](const QJsonObject& json, int, const QString& error) {
        if (!error.isEmpty()) {
            cb(false, tr("Couldn't read the backup: %1").arg(error));
            return;
        }
        const QJsonArray ranges = json.value(QStringLiteral("valueRanges")).toArray();
        auto rows = [&](int i) { return ranges.at(i).toObject().value(QStringLiteral("values")).toArray(); };
        SheetFormat::LogbookData data;
        QString parseError;
        if (ranges.size() < 4 || !SheetFormat::parse(rows(0), rows(1), rows(2), rows(3), &data, &parseError)) {
            cb(false, parseError.isEmpty() ? tr("This spreadsheet isn't a Pilot Logbook backup.") : parseError);
            return;
        }
        // Keep a copy of the current logbook before replacing it.
        const QString safety = QFileInfo(m_db.path()).absolutePath() + QStringLiteral("/logbook.before-restore.db");
        if (!m_db.backupTo(safety)) {
            cb(false, m_db.lastError());
            return;
        }
        QList<QPair<Flight, QString>> flights;
        for (const SheetFormat::RestoredFlight& rf : data.flights)
            flights << qMakePair(rf.flight, rf.registration);
        if (!m_db.replaceAll(data.aircraft, flights, data.broughtForward)) {
            cb(false, m_db.lastError());
            return;
        }
        for (const TimeColumn& c : timeColumns())
            if (c.renamable)
                setColumnLabel(c.key, data.specLabels.value(c.key));
        setSpreadsheetId(id);  // keep backing up to the restored spreadsheet
        emit restored();
        cb(true, {});
    });
}
