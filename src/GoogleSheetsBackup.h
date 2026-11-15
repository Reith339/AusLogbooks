#pragma once
// Keeps a copy of the logbook in a Google Sheets spreadsheet in the user's Drive.
// Upload is one-way and automatic (a few seconds after each change); the spreadsheet
// can be restored onto this or another computer.

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include <functional>

class Database;
class GoogleAuth;
class QNetworkAccessManager;

class GoogleSheetsBackup : public QObject {
    Q_OBJECT
public:
    struct SheetInfo {
        QString id;
        QString name;
        QDateTime modified;
    };
    using FindCallback = std::function<void(const QList<SheetInfo>& sheets, const QString& error)>;
    using DoneCallback = std::function<void(bool ok, const QString& error)>;

    GoogleSheetsBackup(Database& db, GoogleAuth* auth, QNetworkAccessManager* nam, QObject* parent = nullptr);

    QString spreadsheetId() const;
    QUrl spreadsheetUrl() const;
    void setSpreadsheetId(const QString& id);  // empty = create a new one on next backup

    // Back up a few seconds from now (changes made in quick succession are combined).
    void scheduleBackup();
    void backupNow();
    // Waits (up to `msTimeout`) for a pending or running backup to finish. Used when quitting.
    void flush(int msTimeout);

    // Backup spreadsheets this app created in the user's Drive, newest first.
    void findBackups(const FindCallback& cb);
    // Replaces this computer's logbook with the contents of the spreadsheet.
    void restoreFrom(const QString& spreadsheetId, const DoneCallback& cb);

    QString statusText() const;
    bool isBusy() const { return m_busy; }

signals:
    void statusChanged();
    void restored();

private:
    using JsonCallback = std::function<void(const QJsonObject& body, int httpStatus, const QString& error)>;
    void api(const QByteArray& verb, const QUrl& url, const QJsonObject& body, const JsonCallback& cb,
             bool retried = false);
    void runBackup();
    void createSpreadsheet(const std::function<void(const QString& id, const QString& error)>& cb);
    void writeAll(const QString& id, bool allowRecreate);
    void finishBackup(bool ok, const QString& error);

    Database& m_db;
    GoogleAuth* m_auth;
    QNetworkAccessManager* m_nam;
    QTimer m_debounce;
    QTimer m_retry;
    bool m_busy = false;
    bool m_dirty = false;  // changes made while a backup was running
    QDateTime m_lastSuccess;
    QString m_lastError;
};
