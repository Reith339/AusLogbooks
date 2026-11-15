#pragma once
#include <QDialog>

class GoogleAuth;
class GoogleSheetsBackup;
class QLabel;
class QPushButton;

// File → Google Sheets Backup: connect/disconnect a Google account, back up now,
// open the spreadsheet, and restore from it.
class GoogleBackupDialog : public QDialog {
    Q_OBJECT
public:
    GoogleBackupDialog(GoogleAuth* auth, GoogleSheetsBackup* backup, int localFlightCount, QWidget* parent = nullptr);

private:
    void updateUi();
    void connectAccount();
    void afterSignIn();
    void restore();
    void restoreSheet(const QString& id);

    GoogleAuth* m_auth;
    GoogleSheetsBackup* m_backup;
    int m_localFlights;

    QLabel* m_account;
    QLabel* m_status;
    QPushButton* m_connect;
    QPushButton* m_disconnect;
    QPushButton* m_backupNow;
    QPushButton* m_open;
    QPushButton* m_restore;
};
