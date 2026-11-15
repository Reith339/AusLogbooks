#include "Columns.h"

#include <QHash>
#include <QSettings>

const QList<TimeColumn>& timeColumns()
{
    static const QList<TimeColumn> cols = [] {
        using E = TimeColumn::Engine;
        QList<TimeColumn> c;
        const QString day = QStringLiteral("Day"), night = QStringLiteral("Night");
        auto add = [&c](const char* key, const char* group, const char* shortGroup, const QString& label,
                        int number, bool counts, E engine, bool isNight, const QString& role,
                        bool renamable = false) {
            c.append(TimeColumn{QString::fromLatin1(key), QString::fromLatin1(group),
                                QString::fromLatin1(shortGroup), label, number, counts, engine, isNight,
                                role, renamable});
        };

        // Left page: Specialist / Instructor (headings written in by the pilot).
        add("spec_1", "Specialist / Instructor", "Spec/Instr", QStringLiteral("Column A"), 0, false, E::None, false, QString(), true);
        add("spec_2", "Specialist / Instructor", "Spec/Instr", QStringLiteral("Column B"), 0, false, E::None, false, QString(), true);
        add("spec_3", "Specialist / Instructor", "Spec/Instr", QStringLiteral("Column C"), 0, false, E::None, false, QString(), true);

        // Right page, columns (1)-(16).
        add("se_day_icus",   "Single-Engine ICUS",     "SE ICUS",    day,   1, true, E::SE, false, Role::ICUS);
        add("se_night_icus", "Single-Engine ICUS",     "SE ICUS",    night, 2, true, E::SE, true,  Role::ICUS);
        add("se_day_dual",   "Single-Engine Dual",     "SE Dual",    day,   3, true, E::SE, false, Role::Dual);
        add("se_night_dual", "Single-Engine Dual",     "SE Dual",    night, 4, true, E::SE, true,  Role::Dual);
        add("se_day_cmd",    "Single-Engine Command",  "SE Cmd",     day,   5, true, E::SE, false, Role::Command);
        add("se_night_cmd",  "Single-Engine Command",  "SE Cmd",     night, 6, true, E::SE, true,  Role::Command);

        add("me_day_icus",   "Multi-Engine ICUS",      "ME ICUS",    day,   7, true, E::ME, false, Role::ICUS);
        add("me_night_icus", "Multi-Engine ICUS",      "ME ICUS",    night, 8, true, E::ME, true,  Role::ICUS);
        add("me_day_dual",   "Multi-Engine Dual",      "ME Dual",    day,   9, true, E::ME, false, Role::Dual);
        add("me_night_dual", "Multi-Engine Dual",      "ME Dual",    night, 10, true, E::ME, true, Role::Dual);
        add("me_day_cmd",    "Multi-Engine Command",   "ME Cmd",     day,   11, true, E::ME, false, Role::Command);
        add("me_night_cmd",  "Multi-Engine Command",   "ME Cmd",     night, 12, true, E::ME, true, Role::Command);
        add("me_day_cop",    "Multi-Engine Co-pilot",  "ME Co-pilot", day,  13, true, E::ME, false, Role::CoPilot);
        add("me_night_cop",  "Multi-Engine Co-pilot",  "ME Co-pilot", night, 14, true, E::ME, true, Role::CoPilot);

        add("inst_flight",   "Instrument",             "Instrument", QStringLiteral("In Flight"), 15, false, E::None, false, QString());
        add("inst_ground",   "Instrument",             "Instrument", QStringLiteral("Sim"),       16, false, E::None, false, QString());
        return c;
    }();
    return cols;
}

namespace {
const QString kLabelGroup = QStringLiteral("columnLabels/");

QHash<QString, QString>& labelCache()
{
    static QHash<QString, QString> cache = [] {
        QHash<QString, QString> h;
        QSettings s;
        for (const TimeColumn& c : timeColumns())
            if (c.renamable) {
                const QString v = s.value(kLabelGroup + c.key).toString();
                if (!v.isEmpty())
                    h.insert(c.key, v);
            }
        return h;
    }();
    return cache;
}
}  // namespace

QString columnLabel(const TimeColumn& c)
{
    if (!c.renamable)
        return c.label;
    return labelCache().value(c.key, c.label);
}

void setColumnLabel(const QString& key, const QString& label)
{
    const QString l = label.trimmed();
    QSettings s;
    if (l.isEmpty()) {
        labelCache().remove(key);
        s.remove(kLabelGroup + key);
    } else {
        labelCache().insert(key, l);
        s.setValue(kLabelGroup + key, l);
    }
}

QString columnHeader(const TimeColumn& c, const QString& sep)
{
    QString h = c.shortGroup + sep + columnLabel(c);
    if (c.number > 0)
        h += sep + QStringLiteral("(%1)").arg(c.number);
    return h;
}

QString findColumnKey(TimeColumn::Engine engine, bool night, const QString& role)
{
    for (const TimeColumn& c : timeColumns())
        if (c.engine == engine && c.night == night && c.role == role)
            return c.key;
    return {};
}

int flightMinutes(const QMap<QString, int>& minutes)
{
    int total = 0;
    for (const TimeColumn& c : timeColumns())
        if (c.countsToFlightTime)
            total += minutes.value(c.key, 0);
    return total;
}

int aeronauticalMinutes(const QMap<QString, int>& minutes)
{
    return flightMinutes(minutes) + minutes.value(kSimKey, 0);
}
