#include "FlightTableModel.h"

#include "Columns.h"
#include "TimeUtil.h"

#include <QFont>

void FlightTableModel::setFlights(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft,
                                  int broughtForwardMinutes)
{
    beginResetModel();
    m_flights = flights;
    m_aircraft = aircraft;
    m_running.clear();
    int running = broughtForwardMinutes;
    for (const Flight& f : m_flights) {
        running += f.flightTime();
        m_running << running;
    }
    endResetModel();
}

void FlightTableModel::setDecimal(bool decimal)
{
    beginResetModel();
    m_decimal = decimal;
    endResetModel();
}

int FlightTableModel::dayLdgColumn() const
{
    return FirstTimeColumn + int(timeColumns().size());
}

int FlightTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_flights.size());
}

int FlightTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : runningTotalColumn() + 1;
}

QVariant FlightTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const Flight& f = m_flights.at(index.row());
    const int col = index.column();

    if (role == Qt::TextAlignmentRole) {
        if (col >= FirstTimeColumn)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::FontRole && col >= flightTotalColumn()) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};

    const Aircraft ac = m_aircraft.value(f.aircraftId);
    switch (col) {
    case Date: return f.date.toString(QStringLiteral("dd/MM/yyyy"));
    case Type: return f.aircraftId ? ac.type : QStringLiteral("—");
    case Rego: return f.aircraftId ? ac.registration : QString();
    case Pic: return f.pic;
    case Copilot: return f.copilot;
    case Route:
        if (f.dep.isEmpty() && f.arr.isEmpty())
            return QString();
        return f.dep + QStringLiteral(" – ") + f.arr;
    case Details: return f.details;
    default: break;
    }

    if (col < dayLdgColumn())
        return TimeUtil::format(f.value(timeColumns().at(col - FirstTimeColumn).key), m_decimal);
    if (col == dayLdgColumn())
        return f.dayLandings ? QString::number(f.dayLandings) : QString();
    if (col == nightLdgColumn())
        return f.nightLandings ? QString::number(f.nightLandings) : QString();
    if (col == approachesColumn())
        return f.approaches ? QString::number(f.approaches) : QString();
    if (col == flightTotalColumn())
        return TimeUtil::format(f.flightTime(), m_decimal);
    if (col == runningTotalColumn())
        return TimeUtil::format(m_running.at(index.row()), m_decimal, false);
    return {};
}

QVariant FlightTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical) {
        if (role == Qt::DisplayRole)
            return section + 1;
        return {};
    }
    if (role == Qt::ToolTipRole && section >= FirstTimeColumn && section < dayLdgColumn()) {
        const TimeColumn& c = timeColumns().at(section - FirstTimeColumn);
        return c.group + QStringLiteral(" — ") + columnLabel(c);
    }
    if (role != Qt::DisplayRole)
        return {};

    switch (section) {
    case Date: return QStringLiteral("Date");
    case Type: return QStringLiteral("Aircraft\nType");
    case Rego: return QStringLiteral("Aircraft\nRego");
    case Pic: return QStringLiteral("Pilot in\nCommand");
    case Copilot: return QStringLiteral("Other Pilot\nor Crew");
    case Route: return QStringLiteral("Route\nFrom – To");
    case Details: return QStringLiteral("Details of Flight");
    default: break;
    }
    if (section < dayLdgColumn())
        return columnHeader(timeColumns().at(section - FirstTimeColumn), QStringLiteral("\n"));
    if (section == dayLdgColumn()) return QStringLiteral("Landings\nDay");
    if (section == nightLdgColumn()) return QStringLiteral("Landings\nNight");
    if (section == approachesColumn()) return QStringLiteral("Inst\nApps");
    if (section == flightTotalColumn()) return QStringLiteral("Flight\nTotal");
    if (section == runningTotalColumn()) return QStringLiteral("Running\nTotal");
    return {};
}
