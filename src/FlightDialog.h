#pragma once
#include "Database.h"

#include <QDialog>
#include <QHash>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QSpinBox;

class FlightDialog : public QDialog {
    Q_OBJECT
public:
    FlightDialog(Database& db, const Flight& flight, bool decimal, QWidget* parent = nullptr);

    Flight flight() const { return m_flight; }

protected:
    void accept() override;

private:
    void reloadAircraft(qint64 selectId);
    void manageAircraft();
    void quickFill();
    void updateTotal();
    QLineEdit* makeTimeEdit(int minutes);

    Database& m_db;
    Flight m_flight;
    bool m_decimal;

    QDateEdit* m_date;
    QComboBox* m_aircraft;
    QLineEdit *m_pic, *m_copilot, *m_dep, *m_arr, *m_details;
    QLineEdit *m_qTotal, *m_qNight, *m_qInst;
    QComboBox* m_qRole;
    QMap<QString, QLineEdit*> m_timeEdits;
    QSpinBox *m_dayLdg, *m_nightLdg, *m_apps;
    QCheckBox *m_review, *m_ipc;
    QLabel* m_totalLabel;
    QHash<qint64, Aircraft> m_aircraftById;
};
