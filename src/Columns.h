#pragma once
// Defines the time columns of the logbook, matching the CASA logbook layout:
//   left page:  Specialist / Instructor (3 user-named columns)
//   right page: (1)-(14) Single/Multi-engine x ICUS/Dual/Command/Co-pilot x Day/Night,
//               (15) Instrument in flight, (16) Simulator.
// The database, tables, entry form and totals are all generated from the list in
// Columns.cpp; new columns are added to an existing database automatically.

#include <QList>
#include <QMap>
#include <QString>

namespace Role {
inline const QString ICUS    = QStringLiteral("ICUS");
inline const QString Dual    = QStringLiteral("Dual");
inline const QString Command = QStringLiteral("Command");
inline const QString CoPilot = QStringLiteral("Co-pilot");
}

struct TimeColumn {
    enum class Engine { SE, ME, None };

    QString key;              // database column name (never change once used)
    QString group;            // e.g. "Single-Engine ICUS"
    QString shortGroup;       // e.g. "SE ICUS" (table header)
    QString label;            // e.g. "Day" (default label)
    int number;               // column number printed in the logbook, 0 if none
    bool countsToFlightTime;  // part of "Total flying" (cols 1-14)
    Engine engine;
    bool night;
    QString role;             // one of Role::*, or empty
    bool renamable;           // user can set the label (Specialist/Instructor)
};

const QList<TimeColumn>& timeColumns();

// Label to display: the user's name for renamable columns, otherwise the default.
QString columnLabel(const TimeColumn& c);
void setColumnLabel(const QString& key, const QString& label);

// "SE ICUS Day (1)" style header text, with the given separator between parts.
QString columnHeader(const TimeColumn& c, const QString& sep);

// Key of the column matching engine/night/role, or empty if none exists
// (e.g. there is no single-engine co-pilot column).
QString findColumnKey(TimeColumn::Engine engine, bool night, const QString& role);

// Total flying: sum of columns 1-14.
int flightMinutes(const QMap<QString, int>& minutes);
// Total aeronautical experience: columns 1-14 + 16 (simulator).
int aeronauticalMinutes(const QMap<QString, int>& minutes);

inline const QString kSimKey = QStringLiteral("inst_ground");
inline const QString kInstFlightKey = QStringLiteral("inst_flight");
