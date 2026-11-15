#include "Database.h"

#include "Columns.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

QString Aircraft::display() const
{
    return QStringLiteral("%1  (%2, %3)").arg(registration, type, engine);
}

int Flight::flightTime() const
{
    return flightMinutes(minutes);
}

namespace {
// Non-time columns of the flights table, in bind order (after id).
const QStringList kFlightFields = {
    QStringLiteral("date"),        QStringLiteral("aircraft_id"), QStringLiteral("pic"),
    QStringLiteral("copilot"),     QStringLiteral("dep"),         QStringLiteral("arr"),
    QStringLiteral("details"),     QStringLiteral("day_ldg"),     QStringLiteral("night_ldg"),
    QStringLiteral("approaches"),  QStringLiteral("flight_review"), QStringLiteral("ipc"),
};

QStringList allFlightFields()
{
    QStringList f = kFlightFields;
    for (const TimeColumn& c : timeColumns())
        f << c.key;
    return f;
}

void bindFlight(QSqlQuery& q, const Flight& f)
{
    q.addBindValue(f.date.toString(Qt::ISODate));
    q.addBindValue(f.aircraftId);
    q.addBindValue(f.pic);
    q.addBindValue(f.copilot);
    q.addBindValue(f.dep);
    q.addBindValue(f.arr);
    q.addBindValue(f.details);
    q.addBindValue(f.dayLandings);
    q.addBindValue(f.nightLandings);
    q.addBindValue(f.approaches);
    q.addBindValue(f.flightReview ? 1 : 0);
    q.addBindValue(f.ipc ? 1 : 0);
    for (const TimeColumn& c : timeColumns())
        q.addBindValue(f.value(c.key));
}
}  // namespace

bool Database::open(const QString& path)
{
    m_path = path;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }
    return ensureSchema();
}

bool Database::backupTo(const QString& file)
{
    if (QFileInfo::exists(file) && !QFile::remove(file)) {
        m_error = QStringLiteral("Couldn't replace %1").arg(file);
        return false;
    }
    // VACUUM INTO writes a complete, consistent copy even while the database is open.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("VACUUM INTO ?"));
    q.addBindValue(file);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

namespace {
// Returns an empty string if `file` is a readable logbook, otherwise the reason it isn't.
QString checkLogbookFile(const QString& file)
{
    const QString conn = QStringLiteral("restore-check");
    QString problem;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(file);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open()) {
            problem = db.lastError().text();
        } else {
            QSqlQuery q(db);
            if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM flights")) || !q.next()
                || !q.exec(QStringLiteral("SELECT COUNT(*) FROM aircraft")))
                problem = QStringLiteral("This file isn't a Pilot Logbook backup.");
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(conn);
    return problem;
}
}  // namespace

bool Database::restoreFrom(const QString& file)
{
    const QString problem = checkLogbookFile(file);
    if (!problem.isEmpty()) {
        m_error = problem;
        return false;
    }
    const QString safety = QFileInfo(m_path).absolutePath() + QStringLiteral("/logbook.before-restore.db");
    if (!backupTo(safety))
        return false;

    m_db.close();
    QFile::remove(m_path + QStringLiteral("-journal"));
    bool ok = QFile::remove(m_path) && QFile::copy(file, m_path);
    if (ok)
        QFile(m_path).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    else {
        // Put the previous logbook back.
        QFile::remove(m_path);
        QFile::copy(safety, m_path);
        m_error = QStringLiteral("Couldn't copy the backup file into place.");
    }
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }
    return ensureSchema() && ok;
}

bool Database::replaceAll(const QList<Aircraft>& aircraft, const QList<QPair<Flight, QString>>& flights,
                          const QMap<QString, int>& broughtForward)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    auto fail = [this](const QString& context) {
        m_error = context + QStringLiteral(": ") + m_error;
        m_db.rollback();
        return false;
    };
    for (const QString& table : {QStringLiteral("flights"), QStringLiteral("aircraft"), QStringLiteral("brought_forward")})
        if (!exec(QStringLiteral("DELETE FROM %1").arg(table)))
            return fail(QStringLiteral("Clearing %1").arg(table));

    QHash<QString, qint64> idByReg;
    for (Aircraft a : aircraft) {
        a.id = 0;
        if (!saveAircraft(a))
            return fail(QStringLiteral("Adding aircraft %1").arg(a.registration));
        idByReg.insert(a.registration.toUpper(), a.id);
    }
    for (const auto& [flight, reg] : flights) {
        Flight f = flight;
        f.id = 0;
        f.aircraftId = reg.isEmpty() ? 0 : idByReg.value(reg.toUpper(), 0);
        if (!reg.isEmpty() && !f.aircraftId) {
            m_error = QStringLiteral("unknown aircraft %1").arg(reg);
            return fail(QStringLiteral("Adding flight on %1").arg(f.date.toString(Qt::ISODate)));
        }
        if (!saveFlight(f))
            return fail(QStringLiteral("Adding flight on %1").arg(f.date.toString(Qt::ISODate)));
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO brought_forward (key, minutes) VALUES (?, ?)"));
    for (auto it = broughtForward.cbegin(); it != broughtForward.cend(); ++it) {
        q.addBindValue(it.key());
        q.addBindValue(it.value());
        if (!q.exec()) {
            m_error = q.lastError().text();
            return fail(QStringLiteral("Adding brought-forward totals"));
        }
    }
    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        return fail(QStringLiteral("Saving"));
    }
    return true;
}

void Database::autoBackup(const QString& dir, int everyDays, int keep)
{
    QDir d(dir);
    if (!d.exists() && !QDir().mkpath(dir))
        return;
    const QStringList existing =
        d.entryList({QStringLiteral("logbook-auto-*.db")}, QDir::Files, QDir::Name);  // oldest first
    if (!existing.isEmpty()) {
        const QDate last = QDate::fromString(existing.last().mid(13, 10), Qt::ISODate);
        if (last.isValid() && last.daysTo(QDate::currentDate()) < everyDays)
            return;
    }
    const QString name = QStringLiteral("logbook-auto-%1.db").arg(QDate::currentDate().toString(Qt::ISODate));
    if (!backupTo(d.filePath(name)))
        return;
    QStringList all = d.entryList({QStringLiteral("logbook-auto-*.db")}, QDir::Files, QDir::Name);
    while (all.size() > keep)
        d.remove(all.takeFirst());
}

bool Database::exec(const QString& sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::ensureSchema()
{
    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS aircraft ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " registration TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            " type TEXT NOT NULL,"
            " engine TEXT NOT NULL DEFAULT 'SE')")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS flights ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " date TEXT NOT NULL,"
            " aircraft_id INTEGER NOT NULL DEFAULT 0,"
            " pic TEXT, copilot TEXT, dep TEXT, arr TEXT, details TEXT,"
            " day_ldg INTEGER NOT NULL DEFAULT 0,"
            " night_ldg INTEGER NOT NULL DEFAULT 0,"
            " approaches INTEGER NOT NULL DEFAULT 0,"
            " flight_review INTEGER NOT NULL DEFAULT 0,"
            " ipc INTEGER NOT NULL DEFAULT 0)")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS brought_forward ("
            " key TEXT PRIMARY KEY, minutes INTEGER NOT NULL DEFAULT 0)")))
        return false;

    // Add any time columns that don't exist yet (lets the layout grow later).
    QSet<QString> existing;
    QSqlQuery info(m_db);
    if (!info.exec(QStringLiteral("PRAGMA table_info(flights)"))) {
        m_error = info.lastError().text();
        return false;
    }
    while (info.next())
        existing.insert(info.value(1).toString());
    for (const TimeColumn& c : timeColumns()) {
        if (!existing.contains(c.key)) {
            if (!exec(QStringLiteral("ALTER TABLE flights ADD COLUMN %1 INTEGER NOT NULL DEFAULT 0")
                          .arg(c.key)))
                return false;
        }
    }
    return true;
}

QList<Aircraft> Database::aircraft() const
{
    QList<Aircraft> list;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id, registration, type, engine FROM aircraft ORDER BY registration"))) {
        m_error = q.lastError().text();
        return list;
    }
    while (q.next()) {
        Aircraft a;
        a.id = q.value(0).toLongLong();
        a.registration = q.value(1).toString();
        a.type = q.value(2).toString();
        a.engine = q.value(3).toString();
        list << a;
    }
    return list;
}

bool Database::saveAircraft(Aircraft& a)
{
    QSqlQuery q(m_db);
    if (a.id == 0) {
        q.prepare(QStringLiteral("INSERT INTO aircraft (registration, type, engine) VALUES (?, ?, ?)"));
    } else {
        q.prepare(QStringLiteral("UPDATE aircraft SET registration = ?, type = ?, engine = ? WHERE id = ?"));
    }
    q.addBindValue(a.registration);
    q.addBindValue(a.type);
    q.addBindValue(a.engine);
    if (a.id != 0)
        q.addBindValue(a.id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        if (m_error.contains(QStringLiteral("UNIQUE")))
            m_error = QStringLiteral("An aircraft with registration %1 already exists.").arg(a.registration);
        return false;
    }
    if (a.id == 0)
        a.id = q.lastInsertId().toLongLong();
    return true;
}

bool Database::deleteAircraft(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM aircraft WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

int Database::flightCountForAircraft(qint64 id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM flights WHERE aircraft_id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

QList<Flight> Database::flights() const
{
    QList<Flight> list;
    QSqlQuery q(m_db);
    const QString sql = QStringLiteral("SELECT id, %1 FROM flights ORDER BY date, id")
                            .arg(allFlightFields().join(QStringLiteral(", ")));
    if (!q.exec(sql)) {
        m_error = q.lastError().text();
        return list;
    }
    while (q.next()) {
        Flight f;
        int i = 0;
        f.id = q.value(i++).toLongLong();
        f.date = QDate::fromString(q.value(i++).toString(), Qt::ISODate);
        f.aircraftId = q.value(i++).toLongLong();
        f.pic = q.value(i++).toString();
        f.copilot = q.value(i++).toString();
        f.dep = q.value(i++).toString();
        f.arr = q.value(i++).toString();
        f.details = q.value(i++).toString();
        f.dayLandings = q.value(i++).toInt();
        f.nightLandings = q.value(i++).toInt();
        f.approaches = q.value(i++).toInt();
        f.flightReview = q.value(i++).toInt() != 0;
        f.ipc = q.value(i++).toInt() != 0;
        for (const TimeColumn& c : timeColumns()) {
            const int m = q.value(i++).toInt();
            if (m != 0)
                f.minutes.insert(c.key, m);
        }
        list << f;
    }
    return list;
}

bool Database::saveFlight(Flight& f)
{
    const QStringList fields = allFlightFields();
    QSqlQuery q(m_db);
    if (f.id == 0) {
        QStringList marks;
        for (int i = 0; i < fields.size(); ++i)
            marks << QStringLiteral("?");
        q.prepare(QStringLiteral("INSERT INTO flights (%1) VALUES (%2)")
                      .arg(fields.join(QStringLiteral(", ")), marks.join(QStringLiteral(", "))));
    } else {
        QStringList sets;
        for (const QString& field : fields)
            sets << field + QStringLiteral(" = ?");
        q.prepare(QStringLiteral("UPDATE flights SET %1 WHERE id = ?").arg(sets.join(QStringLiteral(", "))));
    }
    bindFlight(q, f);
    if (f.id != 0)
        q.addBindValue(f.id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    if (f.id == 0)
        f.id = q.lastInsertId().toLongLong();
    return true;
}

bool Database::deleteFlight(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM flights WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

QMap<QString, int> Database::broughtForward() const
{
    QMap<QString, int> map;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT key, minutes FROM brought_forward"))) {
        m_error = q.lastError().text();
        return map;
    }
    while (q.next())
        map.insert(q.value(0).toString(), q.value(1).toInt());
    return map;
}

bool Database::setBroughtForward(const QMap<QString, int>& values)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("DELETE FROM brought_forward"))) {
        m_error = q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.prepare(QStringLiteral("INSERT INTO brought_forward (key, minutes) VALUES (?, ?)"));
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value() == 0)
            continue;
        q.addBindValue(it.key());
        q.addBindValue(it.value());
        if (!q.exec()) {
            m_error = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}
