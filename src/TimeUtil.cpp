#include "TimeUtil.h"

#include <QStringList>
#include <QtMath>

namespace TimeUtil {

QString format(int minutes, bool decimal, bool blankZero)
{
    if (minutes == 0 && blankZero)
        return {};
    if (decimal)
        return QString::number(minutes / 60.0, 'f', 1);
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

bool parse(const QString& text, int* minutes)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        *minutes = 0;
        return true;
    }
    if (t.contains(QLatin1Char(':'))) {
        const QStringList parts = t.split(QLatin1Char(':'));
        if (parts.size() != 2)
            return false;
        bool okH = true, okM = false;
        const int h = parts[0].isEmpty() ? 0 : parts[0].toInt(&okH);
        const int m = parts[1].toInt(&okM);
        if (!okH || !okM || h < 0 || m < 0 || m >= 60)
            return false;
        *minutes = h * 60 + m;
        return true;
    }
    bool ok = false;
    const double hours = t.toDouble(&ok);
    if (!ok || hours < 0)
        return false;
    *minutes = qRound(hours * 60.0);
    return true;
}

}
