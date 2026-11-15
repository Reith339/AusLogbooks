#pragma once
#include <QDate>
#include <QList>
#include <QMap>
#include <QSqlDatabase>
#include <QString>

struct Aircraft {
    qint64 id = 0;
    QString registration;   // e.g. VH-ABC
    QString type;           // e.g. C172
    QString engine = QStringLiteral("SE");  // "SE" or "ME"

    QString display() const;
};

struct Flight {
    qint64 id = 0;
    QDate date;
    qint64 aircraftId = 0;  // 0 = none (e.g. simulator session)
    QString pic, copilot, dep, arr, details;
    QMap<QString, int> minutes;  // TimeColumn::key -> minutes
    int dayLandings = 0, nightLandings = 0, approaches = 0;
    bool flightReview = false, ipc = false;

    int value(const QString& key) const { return minutes.value(key, 0); }
    int flightTime() const;
};

class Database {
public:
    bool open(const QString& path);
    QString path() const { return m_path; }
    QString lastError() const { return m_error; }

    // Writes a consistent copy of the logbook to `file` (overwrites it).
    bool backupTo(const QString& file);
    // Replaces the logbook with a backup file after checking it's a valid logbook.
    // The current logbook is kept next to it as "logbook.before-restore.db".
    bool restoreFrom(const QString& file);
    // Replaces all aircraft, flights and brought-forward totals in one transaction.
    // Each flight is paired with its aircraft registration (empty = none / simulator).
    bool replaceAll(const QList<Aircraft>& aircraft, const QList<QPair<Flight, QString>>& flights,
                    const QMap<QString, int>& broughtForward);
    // Keeps rolling automatic backups in `dir`: one per `everyDays`, newest `keep` kept.
    void autoBackup(const QString& dir, int everyDays = 7, int keep = 8);

    QList<Aircraft> aircraft() const;
    bool saveAircraft(Aircraft& a);
    bool deleteAircraft(qint64 id);
    int flightCountForAircraft(qint64 id) const;

    QList<Flight> flights() const;  // logbook order (date, then entry order)
    bool saveFlight(Flight& f);
    bool deleteFlight(qint64 id);

    QMap<QString, int> broughtForward() const;
    bool setBroughtForward(const QMap<QString, int>& values);

private:
    bool exec(const QString& sql);
    bool ensureSchema();

    QSqlDatabase m_db;
    QString m_path;
    mutable QString m_error;
};
