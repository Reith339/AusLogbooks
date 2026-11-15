#include "Recency.h"

#include <algorithm>
#include <functional>

QList<RecencyItem> computeRecency(QList<Flight> flights, const QDate& today)
{
    // Most recent first; ignore future-dated entries.
    flights.erase(std::remove_if(flights.begin(), flights.end(),
                                 [&](const Flight& f) { return !f.date.isValid() || f.date > today; }),
                  flights.end());
    std::sort(flights.begin(), flights.end(), [](const Flight& a, const Flight& b) {
        return a.date != b.date ? a.date > b.date : a.id > b.id;
    });

    // Date of the flight on which the running count (newest backwards) reaches `needed`.
    auto nthEventDate = [&](const std::function<int(const Flight&)>& count, int needed) -> QDate {
        int acc = 0;
        for (const Flight& f : flights) {
            acc += count(f);
            if (acc >= needed)
                return f.date;
        }
        return {};
    };

    auto make = [&](const QString& name, const QString& requirement, int needed,
                    const std::function<int(const Flight&)>& count,
                    const std::function<QDate(const QDate&)>& expiry,
                    const QString& noneText) {
        RecencyItem item;
        item.name = name;
        item.requirement = requirement;
        const QDate basis = nthEventDate(count, needed);
        if (!basis.isValid()) {
            item.status = RecencyItem::Status::NotCurrent;
            item.detail = noneText;
            return item;
        }
        item.validUntil = expiry(basis);
        const qint64 daysLeft = today.daysTo(item.validUntil);
        if (daysLeft < 0)
            item.status = RecencyItem::Status::NotCurrent;
        else if (daysLeft <= kExpiringWarningDays)
            item.status = RecencyItem::Status::Expiring;
        else
            item.status = RecencyItem::Status::Current;

        item.detail = daysLeft < 0
            ? QStringLiteral("Lapsed %1 day(s) ago (based on %2)")
                  .arg(-daysLeft).arg(basis.toString(QStringLiteral("dd/MM/yyyy")))
            : QStringLiteral("%1 day(s) remaining (based on %2)")
                  .arg(daysLeft).arg(basis.toString(QStringLiteral("dd/MM/yyyy")));
        return item;
    };

    auto plusDays = [](int d) { return [d](const QDate& x) { return x.addDays(d); }; };
    auto plusMonths = [](int m) { return [m](const QDate& x) { return x.addMonths(m); }; };

    QList<RecencyItem> items;
    items << make(QStringLiteral("Passenger carriage (day)"),
                  QStringLiteral("3 take-offs and landings in the previous 90 days"), 3,
                  [](const Flight& f) { return f.dayLandings + f.nightLandings; }, plusDays(90),
                  QStringLiteral("Fewer than 3 landings recorded"));
    items << make(QStringLiteral("Passenger carriage (night)"),
                  QStringLiteral("3 night take-offs and landings in the previous 90 days"), 3,
                  [](const Flight& f) { return f.nightLandings; }, plusDays(90),
                  QStringLiteral("Fewer than 3 night landings recorded"));
    items << make(QStringLiteral("Night VFR"),
                  QStringLiteral("1 night take-off and landing in the previous 6 months"), 1,
                  [](const Flight& f) { return f.nightLandings; }, plusMonths(6),
                  QStringLiteral("No night landings recorded"));
    items << make(QStringLiteral("Instrument approaches (IFR)"),
                  QStringLiteral("3 instrument approaches (aircraft or sim) in the previous 90 days"), 3,
                  [](const Flight& f) { return f.approaches; }, plusDays(90),
                  QStringLiteral("Fewer than 3 approaches recorded"));
    items << make(QStringLiteral("Flight review"),
                  QStringLiteral("Flight review within the previous 24 months"), 1,
                  [](const Flight& f) { return f.flightReview ? 1 : 0; }, plusMonths(24),
                  QStringLiteral("No flight review recorded"));
    items << make(QStringLiteral("Instrument proficiency check"),
                  QStringLiteral("IPC within the previous 12 months"), 1,
                  [](const Flight& f) { return f.ipc ? 1 : 0; }, plusMonths(12),
                  QStringLiteral("No IPC recorded"));
    return items;
}
