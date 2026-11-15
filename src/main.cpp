#include "Database.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("LogBook"));
    QApplication::setApplicationName(QStringLiteral("Pilot Logbook"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon_256.png")));
    QApplication::setStyle(QStringLiteral("Fusion"));
    applyTheme(savedTheme());  // Light, Dark or follow the system (Settings)

    // The logbook lives in the user's app-data folder unless "--db <file>" is given:
    //   Windows: %APPDATA%\LogBook\Pilot Logbook\logbook.db
    //   macOS:   ~/Library/Application Support/LogBook/Pilot Logbook/logbook.db
    QString dbPath;
    const QStringList args = QApplication::arguments();
    const int dbArg = int(args.indexOf(QStringLiteral("--db")));
    const bool customDb = dbArg >= 0 && dbArg + 1 < args.size();
    if (customDb) {
        dbPath = args.at(dbArg + 1);
    } else {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        dbPath = dir + QStringLiteral("/logbook.db");
    }

    Database db;
    if (!db.open(dbPath)) {
        QMessageBox::critical(nullptr, QObject::tr("Pilot Logbook"),
                              QObject::tr("Couldn't open the logbook database:\n%1\n\n%2").arg(dbPath, db.lastError()));
        return 1;
    }
    if (!customDb)
        db.autoBackup(QFileInfo(dbPath).absolutePath() + QStringLiteral("/backups"));

    MainWindow w(db);
    w.show();
    return app.exec();
}
