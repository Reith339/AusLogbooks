#include "GoogleBackupDialog.h"

#include "GoogleAuth.h"
#include "GoogleSheetsBackup.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

GoogleBackupDialog::GoogleBackupDialog(GoogleAuth* auth, GoogleSheetsBackup* backup, int localFlightCount,
                                       QWidget* parent)
    : QDialog(parent), m_auth(auth), m_backup(backup), m_localFlights(localFlightCount)
{
    setWindowTitle(tr("Google Sheets Backup"));
    setMinimumWidth(500);

    auto* intro = new QLabel(tr(
        "Keep a copy of your logbook in Google Sheets, in your own Google Drive. Once connected, every change "
        "is backed up automatically. If you lose this computer, install Pilot Logbook on a new one, connect "
        "the same Google account and restore.<br><br>"
        "<span style='color:gray'>The spreadsheet is a backup copy: changes made in Google Sheets aren't "
        "brought back into the app. The app can only see files it creates in your Drive.</span>"));
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);

    m_account = new QLabel;
    m_account->setTextFormat(Qt::RichText);
    m_status = new QLabel;
    m_status->setWordWrap(true);

    m_connect = new QPushButton(tr("Connect Google Account…"));
    m_connect->setDefault(true);
    m_disconnect = new QPushButton(tr("Disconnect"));
    m_backupNow = new QPushButton(tr("Back Up Now"));
    m_open = new QPushButton(tr("Open in Google Sheets"));
    m_restore = new QPushButton(tr("Restore From Google Sheets…"));

    connect(m_connect, &QPushButton::clicked, this, &GoogleBackupDialog::connectAccount);
    connect(m_disconnect, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Disconnect"),
                                  tr("Stop backing up to Google Sheets?\n\nThe spreadsheet stays in your Google "
                                     "Drive; you can reconnect later."))
            != QMessageBox::Yes)
            return;
        m_auth->signOut();
        updateUi();
    });
    connect(m_backupNow, &QPushButton::clicked, m_backup, &GoogleSheetsBackup::backupNow);
    connect(m_open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(m_backup->spreadsheetUrl()); });
    connect(m_restore, &QPushButton::clicked, this, &GoogleBackupDialog::restore);
    connect(m_backup, &GoogleSheetsBackup::statusChanged, this, &GoogleBackupDialog::updateUi);
    connect(m_auth, &GoogleAuth::signedInChanged, this, &GoogleBackupDialog::updateUi);
    connect(m_auth, &GoogleAuth::signInFinished, this, [this](bool ok, const QString& error) {
        if (ok) {
            afterSignIn();
        } else if (!error.isEmpty() && error != tr("Sign-in cancelled.")) {
            QMessageBox::warning(this, tr("Google sign-in"), error);
        }
        updateUi();
    });

    auto* row1 = new QHBoxLayout;
    row1->addWidget(m_connect);
    row1->addWidget(m_disconnect);
    row1->addStretch();
    auto* row2 = new QHBoxLayout;
    row2->addWidget(m_backupNow);
    row2->addWidget(m_open);
    row2->addWidget(m_restore);
    row2->addStretch();

    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addSpacing(8);
    layout->addWidget(m_account);
    layout->addWidget(m_status);
    layout->addLayout(row1);
    layout->addLayout(row2);
    layout->addWidget(close);

    updateUi();
}

void GoogleBackupDialog::updateUi()
{
    const bool configured = GoogleAuth::isConfigured();
    const bool signedIn = m_auth->isSignedIn();
    const bool waiting = m_auth->signInInProgress();
    const bool hasSheet = !m_backup->spreadsheetId().isEmpty();

    if (!configured)
        m_account->setText(tr("<b>Not available:</b> this build of the app doesn't include Google sign-in."));
    else if (waiting)
        m_account->setText(tr("<b>Waiting for you to approve access in your web browser…</b>"));
    else if (signedIn)
        m_account->setText(tr("Connected as <b>%1</b>").arg(m_auth->email().toHtmlEscaped()));
    else
        m_account->setText(tr("<b>Not connected</b>"));
    m_status->setText(signedIn ? m_backup->statusText() : QString());
    m_status->setVisible(signedIn);

    m_connect->setVisible(!signedIn);
    m_connect->setEnabled(configured);
    m_connect->setText(waiting ? tr("Cancel") : tr("Connect Google Account…"));
    m_disconnect->setVisible(signedIn);
    m_backupNow->setEnabled(signedIn && !m_backup->isBusy());
    m_open->setEnabled(signedIn && hasSheet);
    m_restore->setEnabled(signedIn && !m_backup->isBusy());
}

void GoogleBackupDialog::connectAccount()
{
    if (m_auth->signInInProgress()) {
        m_auth->cancelSignIn();
        return;
    }
    m_auth->signIn();
    updateUi();
}

void GoogleBackupDialog::afterSignIn()
{
    // Look for an existing backup (e.g. made on a previous computer) before creating a new one.
    m_backup->findBackups([this](const QList<GoogleSheetsBackup::SheetInfo>& sheets, const QString& error) {
        if (!error.isEmpty() || sheets.isEmpty()) {
            m_backup->setSpreadsheetId(QString());
            m_backup->backupNow();
            return;
        }
        const GoogleSheetsBackup::SheetInfo& latest = sheets.first();
        QMessageBox box(this);
        box.setWindowTitle(tr("Existing backup found"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("Your Google Drive already has a Pilot Logbook backup, last updated %1.")
                        .arg(latest.modified.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
        box.setInformativeText(
            m_localFlights == 0
                ? tr("This computer's logbook is empty. Restore your logbook from the backup?")
                : tr("Restore it onto this computer (replacing the %n flight(s) here), or keep this computer's "
                     "logbook and back it up over the old copy?",
                     nullptr, m_localFlights));
        QPushButton* restoreBtn = box.addButton(tr("Restore From Backup"), QMessageBox::AcceptRole);
        QPushButton* keepBtn = box.addButton(tr("Keep This Computer's Logbook"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(m_localFlights == 0 ? restoreBtn : keepBtn);
        box.exec();
        if (box.clickedButton() == restoreBtn) {
            restoreSheet(latest.id);
        } else if (box.clickedButton() == keepBtn) {
            if (m_localFlights == 0
                && QMessageBox::warning(this, tr("Overwrite backup?"),
                                        tr("This computer's logbook is empty. Backing it up will replace the "
                                           "flights in your Google Sheets backup with an empty logbook.\n\nAre you sure?"),
                                        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                       != QMessageBox::Yes) {
                m_backup->setSpreadsheetId(QString());  // back up to a new spreadsheet instead
                return;
            }
            m_backup->setSpreadsheetId(latest.id);
            m_backup->backupNow();
        } else {
            // Cancel: leave the existing backup untouched. Future backups go to a new spreadsheet,
            // and the old one can still be restored later.
            m_backup->setSpreadsheetId(QString());
        }
    });
}

void GoogleBackupDialog::restore()
{
    m_restore->setEnabled(false);
    m_backup->findBackups([this](const QList<GoogleSheetsBackup::SheetInfo>& sheets, const QString& error) {
        updateUi();
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("Restore"), tr("Couldn't search your Google Drive: %1").arg(error));
            return;
        }
        if (sheets.isEmpty()) {
            QMessageBox::information(this, tr("Restore"), tr("No Pilot Logbook backups were found in this Google account."));
            return;
        }
        QString id = sheets.first().id;
        if (sheets.size() > 1) {
            QStringList names;
            for (const auto& s : sheets)
                names << tr("Backup updated %1").arg(s.modified.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
            bool ok = false;
            const QString choice = QInputDialog::getItem(this, tr("Restore"), tr("Choose a backup:"), names, 0, false, &ok);
            if (!ok)
                return;
            id = sheets.at(names.indexOf(choice)).id;
        }
        if (QMessageBox::warning(this, tr("Restore From Google Sheets"),
                                 tr("This replaces the logbook on this computer (%n flight(s)) with the backup.\n\n"
                                    "A copy of the current logbook is kept as logbook.before-restore.db in the "
                                    "data folder.\n\nContinue?",
                                    nullptr, m_localFlights),
                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
            != QMessageBox::Yes)
            return;
        restoreSheet(id);
    });
}

void GoogleBackupDialog::restoreSheet(const QString& id)
{
    m_restore->setEnabled(false);
    m_backup->restoreFrom(id, [this](bool ok, const QString& error) {
        updateUi();
        if (!ok) {
            QMessageBox::critical(this, tr("Restore failed"), error);
            return;
        }
        QMessageBox::information(this, tr("Restore complete"), tr("Your logbook was restored from Google Sheets."));
    });
}
