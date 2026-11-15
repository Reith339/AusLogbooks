#pragma once
#include "Database.h"

#include <QAbstractTableModel>
#include <QHash>

class FlightTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum FixedColumn { Date, Type, Rego, Pic, Copilot, Route, Details, FirstTimeColumn };

    using QAbstractTableModel::QAbstractTableModel;

    void setFlights(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft,
                    int broughtForwardMinutes);
    void setDecimal(bool decimal);
    const Flight& flightAt(int row) const { return m_flights.at(row); }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    int dayLdgColumn() const;
    int nightLdgColumn() const { return dayLdgColumn() + 1; }
    int approachesColumn() const { return dayLdgColumn() + 2; }
    int flightTotalColumn() const { return dayLdgColumn() + 3; }
    int runningTotalColumn() const { return dayLdgColumn() + 4; }

    QList<Flight> m_flights;
    QList<int> m_running;
    QHash<qint64, Aircraft> m_aircraft;
    bool m_decimal = true;
};
