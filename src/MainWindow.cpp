#include "MainWindow.h"

#include "AircraftDialog.h"
#include "BroughtForwardDialog.h"
#include "Columns.h"
#include "FlightDialog.h"
#include "FlightTableModel.h"
#include "GoogleAuth.h"
#include "GoogleBackupDialog.h"
#include "GoogleSheetsBackup.h"
#include "PageWidget.h"
#include "RecencyWidget.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "TimeUtil.h"
#include "TotalsWidget.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QApplication>
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUrl>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>

MainWindow::MainWindow(Database& db, QWidget* parent) : QMainWindow(parent), m_db(db)
{
    setWindowTitle(tr("Pilot Logbook"));
    resize(1400, 760);
    m_decimal = QSettings().value(QStringLiteral("display/decimal"), true).toBool();

    m_model = new FlightTableModel(this);
    m_model->setDecimal(m_decimal);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->verticalHeader()->setDefaultSectionSize(24);
    connect(m_table, &QTableView::doubleClicked, this, &MainWindow::editFlight);

    m_pages = new PageWidget;
    m_totals = new TotalsWidget;
    m_recency = new RecencyWidget;

    auto* tabs = new QTabWidget;
    tabs->addTab(m_table, tr("Logbook"));
    tabs->addTab(m_pages, tr("Pages"));
    tabs->addTab(m_totals, tr("Totals"));
    tabs->addTab(m_recency, tr("Recency"));
    setCentralWidget(tabs);

    // ---- Actions ----
    auto* add = new QAction(tr("Add Flight…"), this);
    add->setShortcut(QKeySequence::New);
    connect(add, &QAction::triggered, this, &MainWindow::addFlight);

    // Edit/Delete shortcuts only apply while the logbook table has focus.
    auto* edit = new QAction(tr("Edit Flight…"), this);
    edit->setShortcuts({QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    edit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(edit, &QAction::triggered, this, &MainWindow::editFlight);
    auto* del = new QAction(tr("Delete Flight"), this);
    del->setShortcuts({QKeySequence(QKeySequence::Delete), QKeySequence(Qt::Key_Backspace)});
    del->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(del, &QAction::triggered, this, &MainWindow::deleteFlight);
    m_table->addActions({edit, del});

    auto* aircraft = new QAction(tr("Aircraft…"), this);
    connect(aircraft, &QAction::triggered, this, &MainWindow::manageAircraft);
    auto* bf = new QAction(tr("Brought Forward…"), this);
    connect(bf, &QAction::triggered, this, &MainWindow::editBroughtForward);
    // On macOS this becomes "Preferences…" in the application menu.
    auto* settings = new QAction(tr("Settings…"), this);
    settings->setMenuRole(QAction::PreferencesRole);
    settings->setShortcut(QKeySequence(tr("Ctrl+,")));
    connect(settings, &QAction::triggered, this, &MainWindow::openSettings);

    auto* backup = new QAction(tr("Back Up Logbook…"), this);
    backup->setShortcut(QKeySequence::Save);
    connect(backup, &QAction::triggered, this, &MainWindow::backupLogbook);
    auto* restore = new QAction(tr("Restore From Backup…"), this);
    connect(restore, &QAction::triggered, this, &MainWindow::restoreLogbook);
    auto* exportCsvAct = new QAction(tr("Export CSV…"), this);
    connect(exportCsvAct, &QAction::triggered, this, &MainWindow::exportCsv);
    auto* dataFolder = new QAction(tr("Show Data Folder"), this);
    connect(dataFolder, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_db.path()).absolutePath()));
    });
    auto* quit = new QAction(tr("Quit"), this);
    quit->setShortcut(QKeySequence::Quit);
    quit->setMenuRole(QAction::QuitRole);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto* googleBackup = new QAction(tr("Google Sheets Backup…"), this);
    connect(googleBackup, &QAction::triggered, this, &MainWindow::openGoogleBackup);

    auto* about = new QAction(tr("About Pilot Logbook"), this);
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);

    // ---- Menus (on macOS these appear in the system menu bar) ----
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(backup);
    file->addAction(restore);
    file->addAction(googleBackup);
    file->addSeparator();
    file->addAction(exportCsvAct);
    file->addAction(dataFolder);
    file->addSeparator();
    file->addAction(settings);
    file->addSeparator();
    file->addAction(quit);

    QMenu* logbook = menuBar()->addMenu(tr("&Logbook"));
    logbook->addAction(add);
    logbook->addAction(edit);
    logbook->addAction(del);
    logbook->addSeparator();
    logbook->addAction(aircraft);
    logbook->addAction(bf);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(about);

    // ---- Toolbar: the everyday actions ----
    auto* tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tb->addAction(add);
    tb->addAction(edit);
    tb->addAction(del);
    tb->addSeparator();
    tb->addAction(aircraft);
    tb->addAction(bf);
    tb->addSeparator();
    tb->addAction(backup);
    tb->addAction(settings);

    // Follow the OS light/dark setting live when the theme is "System default".
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (savedTheme() == Theme::System) {
            applyTheme(Theme::System);
            refresh();
        }
    });

    // ---- Google Sheets backup ----
    m_nam = new QNetworkAccessManager(this);
    m_google = new GoogleAuth(m_nam, this);
    m_sheets = new GoogleSheetsBackup(m_db, m_google, m_nam, this);
    m_backupStatus = new QLabel;
    m_backupStatus->setTextFormat(Qt::PlainText);
    statusBar()->addPermanentWidget(m_backupStatus);
    auto updateBackupStatus = [this] {
        m_backupStatus->setText(GoogleAuth::isConfigured() ? m_sheets->statusText() : QString());
    };
    connect(m_sheets, &GoogleSheetsBackup::statusChanged, this, updateBackupStatus);
    connect(m_sheets, &GoogleSheetsBackup::restored, this, &MainWindow::refresh);
    updateBackupStatus();

    refresh();
    // Catch up on anything not yet uploaded (e.g. the last session ended while offline).
    m_sheets->scheduleBackup();
}

void MainWindow::dataChanged()
{
    refresh();
    m_sheets->scheduleBackup();
}

void MainWindow::openGoogleBackup()
{
    GoogleBackupDialog dlg(m_google, m_sheets, int(m_flights.size()), this);
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Give a pending Google Sheets backup a few seconds to finish before quitting.
    if (m_google->isSignedIn()) {
        statusBar()->showMessage(tr("Finishing Google Sheets backup…"));
        m_sheets->flush(8000);
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::refresh()
{
    m_flights = m_db.flights();
    QHash<qint64, Aircraft> aircraft;
    for (const Aircraft& a : m_db.aircraft())
        aircraft.insert(a.id, a);
    const QMap<QString, int> bf = m_db.broughtForward();

    m_model->setFlights(m_flights, aircraft, flightMinutes(bf));
    m_table->resizeColumnsToContents();
    m_table->scrollToBottom();
    m_pages->refresh(m_flights, aircraft, bf, m_decimal);
    m_totals->refresh(m_flights, bf, m_decimal);
    m_recency->refresh(m_flights);

    int flying = flightMinutes(bf), aero = aeronauticalMinutes(bf);
    for (const Flight& f : m_flights) {
        flying += f.flightTime();
        aero += aeronauticalMinutes(f.minutes);
    }
    statusBar()->showMessage(
        tr("%n flight(s)  ·  Grand total flying %1 hrs  ·  Total aeronautical experience %2 hrs", nullptr,
           int(m_flights.size()))
            .arg(TimeUtil::format(flying, m_decimal, false), TimeUtil::format(aero, m_decimal, false)));
}

int MainWindow::selectedRow() const
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::addFlight()
{
    FlightDialog dlg(m_db, Flight{}, m_decimal, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    Flight f = dlg.flight();
    if (!m_db.saveFlight(f)) {
        QMessageBox::critical(this, tr("Error"), m_db.lastError());
        return;
    }
    dataChanged();
}

void MainWindow::editFlight()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    FlightDialog dlg(m_db, m_model->flightAt(row), m_decimal, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    Flight f = dlg.flight();
    if (!m_db.saveFlight(f)) {
        QMessageBox::critical(this, tr("Error"), m_db.lastError());
        return;
    }
    dataChanged();
    m_table->selectRow(row);
}

void MainWindow::deleteFlight()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    const Flight& f = m_model->flightAt(row);
    if (QMessageBox::question(this, tr("Delete flight"),
                              tr("Delete the flight on %1?").arg(f.date.toString(QStringLiteral("dd/MM/yyyy"))))
        != QMessageBox::Yes)
        return;
    if (!m_db.deleteFlight(f.id))
        QMessageBox::critical(this, tr("Error"), m_db.lastError());
    dataChanged();
}

void MainWindow::manageAircraft()
{
    AircraftDialog dlg(m_db, this);
    dlg.exec();
    dataChanged();
}

void MainWindow::editBroughtForward()
{
    BroughtForwardDialog dlg(m_db.broughtForward(), m_decimal, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (!m_db.setBroughtForward(dlg.values()))
        QMessageBox::critical(this, tr("Error"), m_db.lastError());
    dataChanged();
}

void MainWindow::openSettings()
{
    SettingsDialog::Values current;
    current.theme = savedTheme();
    current.decimal = m_decimal;
    current.rowsPerPage = PageWidget::rowsPerPageSetting();
    for (const TimeColumn& c : timeColumns())
        if (c.renamable)
            current.specLabels.insert(c.key, columnLabel(c) == c.label ? QString() : columnLabel(c));

    SettingsDialog dlg(current, this);
    if (dlg.exec() != QDialog::Accepted) {
        refresh();  // repaint in case a theme preview was cancelled
        return;
    }
    const SettingsDialog::Values v = dlg.values();
    saveTheme(v.theme);
    applyTheme(v.theme);
    for (auto it = v.specLabels.cbegin(); it != v.specLabels.cend(); ++it)
        setColumnLabel(it.key(), it.value());
    m_pages->setRowsPerPage(v.rowsPerPage);
    setDecimal(v.decimal);  // saves the setting and refreshes everything
    m_sheets->scheduleBackup();  // column names are part of the backup
}

void MainWindow::backupLogbook()
{
    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/Logbook backup %1.db").arg(QDate::currentDate().toString(Qt::ISODate));
    const QString path = QFileDialog::getSaveFileName(this, tr("Back Up Logbook"), suggested,
                                                      tr("Logbook backup (*.db)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(m_db.path()).absoluteFilePath()) {
        QMessageBox::warning(this, tr("Back Up Logbook"), tr("Choose a different file from the logbook itself."));
        return;
    }
    if (!m_db.backupTo(path)) {
        QMessageBox::critical(this, tr("Backup failed"), m_db.lastError());
        return;
    }
    QMessageBox::information(this, tr("Backup saved"),
                             tr("Your logbook was backed up to:\n%1\n\nKeep a copy somewhere safe, such as a "
                                "USB drive or cloud storage. Use File → Restore From Backup to load it on this "
                                "or another computer.")
                                 .arg(QDir::toNativeSeparators(path)));
}

void MainWindow::restoreLogbook()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Restore From Backup"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Logbook backup (*.db)"));
    if (path.isEmpty())
        return;
    if (QMessageBox::warning(this, tr("Restore From Backup"),
                             tr("This replaces your current logbook (%n flight(s)) with the backup.\n\n"
                                "A copy of the current logbook is kept as logbook.before-restore.db in the data "
                                "folder.\n\nContinue?",
                                nullptr, int(m_flights.size())),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;
    const bool ok = m_db.restoreFrom(path);
    dataChanged();
    if (!ok) {
        QMessageBox::critical(this, tr("Restore failed"), m_db.lastError());
        return;
    }
    QMessageBox::information(this, tr("Restore complete"),
                             tr("Restored %n flight(s) from the backup.", nullptr, int(m_flights.size())));
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About Pilot Logbook"));
    box.setIconPixmap(QPixmap(QStringLiteral(":/icon_256.png")).scaled(96, 96, Qt::KeepAspectRatio,
                                                                       Qt::SmoothTransformation));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<h3>Pilot Logbook %1</h3>"
                   "<p>A digital pilot logbook in the CASA logbook format.</p>"
                   "<p>Your logbook is stored only on this computer:<br><code>%2</code></p>"
                   "<p>Back it up regularly with <b>File → Back Up Logbook</b>. Automatic weekly copies are "
                   "also kept in the <code>backups</code> folder next to it.</p>"
                   "<p style='color:gray'>Recency calculations are a guide only; always confirm against "
                   "CASR Part 61.</p>")
                    .arg(QApplication::applicationVersion(), QDir::toNativeSeparators(m_db.path()).toHtmlEscaped()));
    box.exec();
}

void MainWindow::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), QStringLiteral("logbook.csv"),
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), file.errorString());
        return;
    }
    auto quote = [](QString s) {
        s.replace(QLatin1Char('\n'), QLatin1Char(' '));
        if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))) {
            s.replace(QStringLiteral("\""), QStringLiteral("\"\""));
            s = QLatin1Char('"') + s + QLatin1Char('"');
        }
        return s;
    };
    QTextStream out(&file);
    QStringList header;
    for (int c = 0; c < m_model->columnCount(); ++c)
        header << quote(m_model->headerData(c, Qt::Horizontal).toString());
    out << header.join(QLatin1Char(',')) << '\n';
    for (int r = 0; r < m_model->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_model->columnCount(); ++c)
            row << quote(m_model->data(m_model->index(r, c)).toString());
        out << row.join(QLatin1Char(',')) << '\n';
    }
    statusBar()->showMessage(tr("Exported %n flight(s) to %1", nullptr, m_model->rowCount()).arg(path), 5000);
}

void MainWindow::setDecimal(bool decimal)
{
    m_decimal = decimal;
    QSettings().setValue(QStringLiteral("display/decimal"), decimal);
    m_model->setDecimal(decimal);
    refresh();
}
