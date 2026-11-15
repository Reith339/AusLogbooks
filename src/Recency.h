#pragma once
#include "Database.h"

#include <QDate>
#include <QList>
#include <QString>

struct RecencyItem {
    enum class Status { Current, Expiring, NotCurrent };

    QString name;
    QString requirement;
    Status status = Status::NotCurrent;
    QDate validUntil;   // invalid if no qualifying record
    QString detail;
};

// Days before expiry at which an item is flagged as "expiring".
constexpr int kExpiringWarningDays = 14;

QList<RecencyItem> computeRecency(QList<Flight> flights, const QDate& today);
