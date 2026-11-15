#include "SheetFormat.h"

#include "Columns.h"
#include "TimeUtil.h"

#include <QJsonValue>
#include <QtMath>

namespace SheetFormat {
namespace {

// Fixed columns of the Flights tab, before the time columns.
const QStringList kLeading = {
    QStringLiteral("Date"),          QStringLiteral("Aircraft Reg"), QStringLiteral("Aircraft Type"),
    QStringLiteral("Engine"),        QStringLiteral("Pilot in Command"),
    QStringLiteral("Other Pilot or Crew"), QStringLiteral("From"), QStringLiteral("To"),
    QStringLiteral("Details"),
};
// Fixed columns after the time columns.
const QString kDayLdg = QStringLiteral("Day Landings");
const QString kNightLdg = QStringLiteral("Night Landings");
const QString kApproaches = QStringLiteral("Instrument Approaches");
const QString kReview = QStringLiteral("Flight Review");
const QString kIpc = QStringLiteral("IPC");
const QString kTotal = QStringLiteral("Total Flying (hrs)");

// Header that identifies a time column when reading back. Custom Specialist names are
// appended after " – " so renaming a column never breaks restore.
QString stableHeader(const TimeColumn& c)
{
    QString h = c.shortGroup + QLatin1Char(' ') + c.label;
    if (c.number > 0)
        h += QStringLiteral(" (%1)").arg(c.number);
    return h;
}

QString displayHeader(const TimeColumn& c)
{
    const QString custom = columnLabel(c);
    return custom == c.label ? stableHeader(c) : stableHeader(c) + QStringLiteral(" – ") + custom;
}

QJsonValue hours(int minutes)
{
    return minutes ? QJsonValue(minutes / 60.0) : QJsonValue(QString());
}

QJsonValue count(int n)
{
    return n ? QJsonValue(n) : QJsonValue(QString());
}

QJsonArray row(std::initializer_list<QJsonValue> values)
{
    QJsonArray a;
    for (const QJsonValue& v : values)
        a.append(v);
    return a;
}

// ---- Reading helpers ----
QString text(const QJsonArray& r, int i)
{
    if (i < 0 || i >= r.size())
        return {};
    const QJsonValue v = r.at(i);
    if (v.isString())
        return v.toString().trimmed();
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    if (v.isBool())
        return v.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    return {};
}

// Hours cell -> minutes. Accepts numbers (hours) and text like "1.3" or "1:18".
bool minutesAt(const QJsonArray& r, int i, int* minutes)
{
    *minutes = 0;
    if (i < 0 || i >= r.size())
        return true;
    const QJsonValue v = r.at(i);
    if (v.isDouble()) {
        if (v.toDouble() < 0)
            return false;
        *minutes = qRound(v.toDouble() * 60.0);
        return true;
    }
    return TimeUtil::parse(v.toString(), minutes);
}

int intAt(const QJsonArray& r, int i)
{
    if (i < 0 || i >= r.size())
        return 0;
    const QJsonValue v = r.at(i);
    return qBound(0, v.isDouble() ? int(v.toDouble()) : v.toString().trimmed().toInt(), 99);
}

bool boolAt(const QJsonArray& r, int i)
{
    const QString t = text(r, i).toLower();
    return t == QLatin1String("yes") || t == QLatin1String("true") || t == QLatin1String("y")
        || t == QLatin1String("1");
}

QDate dateAt(const QJsonArray& r, int i)
{
    if (i < 0 || i >= r.size())
        return {};
    const QJsonValue v = r.at(i);
    if (v.isDouble())  // spreadsheet serial date (days since 1899-12-30)
        return QDate(1899, 12, 30).addDays(qint64(v.toDouble()));
    const QString t = v.toString().trimmed();
    QDate d = QDate::fromString(t, Qt::ISODate);
    if (!d.isValid())
        d = QDate::fromString(t, QStringLiteral("d/M/yyyy"));
    return d;
}

QHash<QString, int> headerIndex(const QJsonArray& rows)
{
    QHash<QString, int> idx;
    if (rows.isEmpty())
        return idx;
    const QJsonArray header = rows.at(0).toArray();
    for (int i = 0; i < header.size(); ++i)
        idx.insert(header.at(i).toString().trimmed(), i);
    return idx;
}

}  // namespace

QJsonArray flightRows(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft)
{
    QJsonArray rows;
    QJsonArray header;
    for (const QString& h : kLeading)
        header.append(h);
    for (const TimeColumn& c : timeColumns())
        header.append(displayHeader(c));
    for (const QString& h : {kDayLdg, kNightLdg, kApproaches, kReview, kIpc, kTotal})
        header.append(h);
    rows.append(header);

    for (const Flight& f : flights) {
        const Aircraft ac = aircraft.value(f.aircraftId);
        QJsonArray r = row({f.date.toString(Qt::ISODate), f.aircraftId ? ac.registration : QString(),
                            f.aircraftId ? ac.type : QString(), f.aircraftId ? ac.engine : QString(), f.pic,
                            f.copilot, f.dep, f.arr, f.details});
        for (const TimeColumn& c : timeColumns())
            r.append(hours(f.value(c.key)));
        r.append(count(f.dayLandings));
        r.append(count(f.nightLandings));
        r.append(count(f.approaches));
        r.append(f.flightReview ? QStringLiteral("Yes") : QString());
        r.append(f.ipc ? QStringLiteral("Yes") : QString());
        r.append(hours(f.flightTime()));
        rows.append(r);
    }
    return rows;
}

QList<int> flightHourColumns()
{
    QList<int> cols;
    const int first = int(kLeading.size());
    for (int i = 0; i < timeColumns().size(); ++i)
        cols << first + i;
    cols << flightColumnCount() - 1;  // Total Flying
    return cols;
}

int flightColumnCount()
{
    return int(kLeading.size() + timeColumns().size() + 6);
}

QJsonArray aircraftRows(const QList<Aircraft>& aircraft)
{
    QJsonArray rows;
    rows.append(row({QStringLiteral("Registration"), QStringLiteral("Type"), QStringLiteral("Engine (SE/ME)")}));
    for (const Aircraft& a : aircraft)
        rows.append(row({a.registration, a.type, a.engine}));
    return rows;
}

QJsonArray broughtForwardRows(const QMap<QString, int>& bf)
{
    QJsonArray rows;
    rows.append(row({QStringLiteral("Column"), QStringLiteral("Key"), QStringLiteral("Hours")}));
    for (const TimeColumn& c : timeColumns())
        rows.append(row({displayHeader(c), c.key, hours(bf.value(c.key))}));
    return rows;
}

QJsonArray settingsRows(const QMap<QString, QString>& specLabels, const QString& appVersion,
                        const QDateTime& backedUpAt)
{
    QJsonArray rows;
    rows.append(row({QStringLiteral("Setting"), QStringLiteral("Value")}));
    rows.append(row({QStringLiteral("Last backup"), backedUpAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))}));
    rows.append(row({QStringLiteral("App version"), appVersion}));
    for (const TimeColumn& c : timeColumns())
        if (c.renamable)
            rows.append(row({QStringLiteral("label:") + c.key, specLabels.value(c.key)}));
    return rows;
}

bool parse(const QJsonArray& flights, const QJsonArray& aircraft, const QJsonArray& broughtForward,
           const QJsonArray& settings, LogbookData* out, QString* error)
{
    LogbookData data;

    // ---- Aircraft tab ----
    QHash<QString, Aircraft> byReg;  // upper-case registration -> aircraft
    QStringList order;
    auto addAircraft = [&](const QString& reg, const QString& type, const QString& engine) {
        const QString key = reg.toUpper();
        if (key.isEmpty() || byReg.contains(key))
            return;
        Aircraft a;
        a.registration = key;
        a.type = type.isEmpty() ? QStringLiteral("UNKNOWN") : type.toUpper();
        a.engine = engine.toUpper() == QLatin1String("ME") ? QStringLiteral("ME") : QStringLiteral("SE");
        byReg.insert(key, a);
        order << key;
    };
    for (int r = 1; r < aircraft.size(); ++r) {
        const QJsonArray row = aircraft.at(r).toArray();
        addAircraft(text(row, 0), text(row, 1), text(row, 2));
    }

    // ---- Flights tab ----
    const QHash<QString, int> h = headerIndex(flights);
    if (!flights.isEmpty() && !h.contains(QStringLiteral("Date"))) {
        *error = QStringLiteral("The Flights tab doesn't have the expected headings.");
        return false;
    }
    QHash<QString, int> timeIdx;  // column key -> index
    for (auto it = h.cbegin(); it != h.cend(); ++it)
        for (const TimeColumn& c : timeColumns())
            if (it.key().startsWith(stableHeader(c)))
                timeIdx.insert(c.key, it.value());
    auto col = [&](const QString& name) { return h.value(name, -1); };

    for (int r = 1; r < flights.size(); ++r) {
        const QJsonArray row = flights.at(r).toArray();
        bool blank = true;
        for (const QJsonValue& v : row)
            if (!(v.isString() && v.toString().trimmed().isEmpty()) && !v.isNull())
                blank = false;
        if (blank)
            continue;

        RestoredFlight rf;
        Flight& f = rf.flight;
        f.date = dateAt(row, col(QStringLiteral("Date")));
        if (!f.date.isValid()) {
            *error = QStringLiteral("Row %1 of the Flights tab has an invalid date.").arg(r + 1);
            return false;
        }
        rf.registration = text(row, col(QStringLiteral("Aircraft Reg"))).toUpper();
        addAircraft(rf.registration, text(row, col(QStringLiteral("Aircraft Type"))),
                    text(row, col(QStringLiteral("Engine"))));
        f.pic = text(row, col(QStringLiteral("Pilot in Command")));
        f.copilot = text(row, col(QStringLiteral("Other Pilot or Crew")));
        f.dep = text(row, col(QStringLiteral("From")));
        f.arr = text(row, col(QStringLiteral("To")));
        f.details = text(row, col(QStringLiteral("Details")));
        for (auto it = timeIdx.cbegin(); it != timeIdx.cend(); ++it) {
            int m = 0;
            if (!minutesAt(row, it.value(), &m)) {
                *error = QStringLiteral("Row %1 of the Flights tab has an invalid time.").arg(r + 1);
                return false;
            }
            if (m)
                f.minutes.insert(it.key(), m);
        }
        f.dayLandings = intAt(row, col(kDayLdg));
        f.nightLandings = intAt(row, col(kNightLdg));
        f.approaches = intAt(row, col(kApproaches));
        f.flightReview = boolAt(row, col(kReview));
        f.ipc = boolAt(row, col(kIpc));
        data.flights << rf;
    }
    for (const QString& reg : order)
        data.aircraft << byReg.value(reg);

    // ---- Brought Forward tab (by key) ----
    for (int r = 1; r < broughtForward.size(); ++r) {
        const QJsonArray row = broughtForward.at(r).toArray();
        const QString key = text(row, 1);
        int m = 0;
        if (!key.isEmpty() && minutesAt(row, 2, &m) && m)
            data.broughtForward.insert(key, m);
    }

    // ---- Settings tab ----
    for (int r = 1; r < settings.size(); ++r) {
        const QJsonArray row = settings.at(r).toArray();
        const QString key = text(row, 0);
        if (key.startsWith(QLatin1String("label:")))
            data.specLabels.insert(key.mid(6), text(row, 1));
    }

    *out = data;
    return true;
}

}  // namespace SheetFormat
