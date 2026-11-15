#pragma once
#include <QString>

// Light / dark appearance. "System" follows the operating system's setting.
enum class Theme { System, Light, Dark };

Theme savedTheme();
void saveTheme(Theme theme);

// Applies the theme's palette to the whole application.
void applyTheme(Theme theme);

// True when the dark palette is currently in use.
bool isDarkActive();

QString themeName(Theme theme);
