#pragma once
#include "Database.h"

#include <QDialog>

class QTableWidget;

// Manage the list of aircraft (registration + type) used by the flight dropdown.
class AircraftDialog : public QDialog {
    Q_OBJECT
public:
    explicit AircraftDialog(Database& db, QWidget* parent = nullptr);

    // Shows the add/edit form for one aircraft. Returns true if saved.
    static bool editAircraft(Database& db, Aircraft& a, QWidget* parent);

private:
    void reload();
    void addAircraft();
    void editSelected();
    void deleteSelected();
    qint64 selectedId() const;

    Database& m_db;
    QTableWidget* m_table;
    QList<Aircraft> m_list;
};
