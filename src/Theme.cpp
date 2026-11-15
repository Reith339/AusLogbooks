#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

namespace {
const QString kThemeKey = QStringLiteral("display/theme");
bool g_dark = false;

QPalette lightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xf3, 0xf4, 0xf6));
    p.setColor(QPalette::WindowText, QColor(0x1b, 0x1f, 0x24));
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor(0xf4, 0xf6, 0xf9));
    p.setColor(QPalette::Text, QColor(0x1b, 0x1f, 0x24));
    p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x90, 0x99));
    p.setColor(QPalette::Button, QColor(0xec, 0xee, 0xf1));
    p.setColor(QPalette::ButtonText, QColor(0x1b, 0x1f, 0x24));
    p.setColor(QPalette::BrightText, QColor(0xc0, 0x26, 0x26));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor(0x1b, 0x1f, 0x24));
    p.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xc0));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x1f, 0x5f, 0xae));
    p.setColor(QPalette::Light, Qt::white);
    p.setColor(QPalette::Midlight, QColor(0xf7, 0xf8, 0xfa));
    p.setColor(QPalette::Mid, QColor(0xc4, 0xc8, 0xce));
    p.setColor(QPalette::Dark, QColor(0xa0, 0xa5, 0xac));
    p.setColor(QPalette::Shadow, QColor(0x6b, 0x70, 0x78));
    const QColor disabled(0x9a, 0xa0, 0xa8);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xc4, 0xc8, 0xce));
    return p;
}

QPalette darkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x20, 0x22, 0x25));
    p.setColor(QPalette::WindowText, QColor(0xe6, 0xe8, 0xeb));
    p.setColor(QPalette::Base, QColor(0x1a, 0x1b, 0x1e));
    p.setColor(QPalette::AlternateBase, QColor(0x24, 0x26, 0x2a));
    p.setColor(QPalette::Text, QColor(0xe6, 0xe8, 0xeb));
    p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8f, 0x98));
    p.setColor(QPalette::Button, QColor(0x2b, 0x2d, 0x31));
    p.setColor(QPalette::ButtonText, QColor(0xe6, 0xe8, 0xeb));
    p.setColor(QPalette::BrightText, QColor(0xef, 0x6b, 0x6b));
    p.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2d, 0x31));
    p.setColor(QPalette::ToolTipText, QColor(0xe6, 0xe8, 0xeb));
    p.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xc0));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x5e, 0xa0, 0xef));
    p.setColor(QPalette::Light, QColor(0x3a, 0x3d, 0x42));
    p.setColor(QPalette::Midlight, QColor(0x31, 0x33, 0x38));
    p.setColor(QPalette::Mid, QColor(0x26, 0x28, 0x2c));
    p.setColor(QPalette::Dark, QColor(0x14, 0x15, 0x17));
    p.setColor(QPalette::Shadow, QColor(0x0b, 0x0b, 0x0c));
    const QColor disabled(0x7a, 0x7f, 0x87);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3a, 0x3d, 0x42));
    return p;
}
}  // namespace

Theme savedTheme()
{
    const QString v = QSettings().value(kThemeKey).toString();
    if (v == QLatin1String("light"))
        return Theme::Light;
    if (v == QLatin1String("dark"))
        return Theme::Dark;
    return Theme::System;
}

void saveTheme(Theme theme)
{
    QSettings().setValue(kThemeKey, theme == Theme::Light  ? QStringLiteral("light")
                                    : theme == Theme::Dark ? QStringLiteral("dark")
                                                           : QStringLiteral("system"));
}

void applyTheme(Theme theme)
{
    static bool applying = false;  // setColorScheme can re-trigger colorSchemeChanged
    if (applying)
        return;
    applying = true;

    QStyleHints* hints = QGuiApplication::styleHints();
    // Tell the OS which scheme we use, so native parts (e.g. the Windows title bar) match.
    switch (theme) {
    case Theme::Light: hints->setColorScheme(Qt::ColorScheme::Light); break;
    case Theme::Dark: hints->setColorScheme(Qt::ColorScheme::Dark); break;
    case Theme::System: hints->unsetColorScheme(); break;
    }
    g_dark = theme == Theme::Dark || (theme == Theme::System && hints->colorScheme() == Qt::ColorScheme::Dark);
    QApplication::setPalette(g_dark ? darkPalette() : lightPalette());

    applying = false;
}

bool isDarkActive()
{
    return g_dark;
}

QString themeName(Theme theme)
{
    switch (theme) {
    case Theme::Light: return QApplication::translate("Theme", "Light");
    case Theme::Dark: return QApplication::translate("Theme", "Dark");
    case Theme::System: break;
    }
    return QApplication::translate("Theme", "System default");
}
