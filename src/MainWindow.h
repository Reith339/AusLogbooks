#pragma once
#include "Database.h"

#include <QMainWindow>

class FlightTableModel;
class GoogleAuth;
class GoogleSheetsBackup;
class PageWidget;
class QLabel;
class QNetworkAccessManager;
class QTableView;
class RecencyWidget;
class TotalsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Database& db, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void refresh();
    void dataChanged();  // refresh + schedule a Google Sheets backup
    void addFlight();
    void editFlight();
    void deleteFlight();
    void manageAircraft();
    void editBroughtForward();
    void openSettings();
    void openGoogleBackup();
    void exportCsv();
    void backupLogbook();
    void restoreLogbook();
    void showAbout();
    void setDecimal(bool decimal);
    int selectedRow() const;

    Database& m_db;
    bool m_decimal = true;
    FlightTableModel* m_model;
    QTableView* m_table;
    PageWidget* m_pages;
    TotalsWidget* m_totals;
    RecencyWidget* m_recency;
    QList<Flight> m_flights;

    QNetworkAccessManager* m_nam = nullptr;
    GoogleAuth* m_google = nullptr;
    GoogleSheetsBackup* m_sheets = nullptr;
    QLabel* m_backupStatus = nullptr;
};
