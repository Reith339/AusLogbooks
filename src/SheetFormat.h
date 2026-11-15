#pragma once
// Converts the logbook to and from Google Sheets cell values.
// The spreadsheet has four tabs: Flights, Aircraft, Brought Forward, Settings.
// Times are written as hours (minutes / 60, unrounded) so they can be read back exactly.

#include "Database.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QMap>
#include <QString>

namespace SheetFormat {

inline const QString kSpreadsheetTitle = QStringLiteral("Pilot Logbook backup");
inline const QString kFlights = QStringLiteral("Flights");
inline const QString kAircraft = QStringLiteral("Aircraft");
inline const QString kBroughtForward = QStringLiteral("Brought Forward");
inline const QString kSettings = QStringLiteral("Settings");

// A restored flight plus the registration of its aircraft (empty = simulator / none).
struct RestoredFlight {
    Flight flight;
    QString registration;
};

struct LogbookData {
    QList<Aircraft> aircraft;
    QList<RestoredFlight> flights;
    QMap<QString, int> broughtForward;
    QMap<QString, QString> specLabels;  // column key -> custom label
};

// ---- Writing (each returns rows: a QJsonArray of QJsonArrays) ----
QJsonArray flightRows(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft);
QJsonArray aircraftRows(const QList<Aircraft>& aircraft);
QJsonArray broughtForwardRows(const QMap<QString, int>& bf);
QJsonArray settingsRows(const QMap<QString, QString>& specLabels, const QString& appVersion,
                        const QDateTime& backedUpAt);

// 0-based column indexes in the Flights tab that hold hours (for number formatting).
QList<int> flightHourColumns();
int flightColumnCount();

// ---- Reading (rows as returned by the Sheets API with UNFORMATTED_VALUE) ----
bool parse(const QJsonArray& flights, const QJsonArray& aircraft, const QJsonArray& broughtForward,
           const QJsonArray& settings, LogbookData* out, QString* error);

}  // namespace SheetFormat
