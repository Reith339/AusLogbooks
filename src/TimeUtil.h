#pragma once
#include <QString>

// Times are stored as whole minutes and shown as decimal hours (1.3) or hh:mm (1:18).
namespace TimeUtil {

QString format(int minutes, bool decimal, bool blankZero = true);

// Accepts "", "1", "1.3", "1:18". Returns false on invalid input.
bool parse(const QString& text, int* minutes);

}
