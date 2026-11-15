#include "TotalsWidget.h"

#include "Columns.h"
#include "TimeUtil.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QTableWidget* makeTable(const QStringList& headers, QWidget* parent)
{
    auto* t = new QTableWidget(0, int(headers.size()), parent);
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setAlternatingRowColors(true);
    return t;
}

QTableWidgetItem* cell(const QString& text, bool right = false, bool bold = false)
{
    auto* item = new QTableWidgetItem(text);
    if (right)
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (bold) {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
    }
    return item;
}
}  // namespace

TotalsWidget::TotalsWidget(QWidget* parent) : QWidget(parent)
{
    m_columns = makeTable({tr("Column"), tr("Brought forward"), tr("This logbook"), tr("Total")}, this);
    m_periods = makeTable({tr("Period"), tr("Flight time"), tr("Night"), tr("Instrument"), tr("Landings")}, this);

    auto* left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("<b>Totals by column</b>")));
    left->addWidget(m_columns);
    auto* right = new QVBoxLayout;
    right->addWidget(new QLabel(tr("<b>Recent activity</b> (this logbook only)")));
    right->addWidget(m_periods);

    auto* layout = new QHBoxLayout(this);
    layout->addLayout(left, 3);
    layout->addLayout(right, 2);
}

void TotalsWidget::refresh(const QList<Flight>& flights, const QMap<QString, int>& bf, bool decimal)
{
    // ---- Per-column totals ----
    QMap<QString, int> logged;
    int dayLdg = 0, nightLdg = 0, apps = 0;
    for (const Flight& f : flights) {
        for (auto it = f.minutes.cbegin(); it != f.minutes.cend(); ++it)
            logged[it.key()] += it.value();
        dayLdg += f.dayLandings;
        nightLdg += f.nightLandings;
        apps += f.approaches;
    }

    const auto& cols = timeColumns();
    m_columns->setRowCount(int(cols.size()) + 5);
    int r = 0;
    for (const TimeColumn& c : cols) {
        const int b = bf.value(c.key), l = logged.value(c.key);
        QString label = c.group + QStringLiteral(" — ") + columnLabel(c);
        if (c.number)
            label = QStringLiteral("(%1)  ").arg(c.number) + label;
        m_columns->setItem(r, 0, cell(label));
        m_columns->setItem(r, 1, cell(TimeUtil::format(b, decimal), true));
        m_columns->setItem(r, 2, cell(TimeUtil::format(l, decimal), true));
        m_columns->setItem(r, 3, cell(TimeUtil::format(b + l, decimal), true, true));
        ++r;
    }
    const int bfTotal = flightMinutes(bf), loggedTotal = flightMinutes(logged);
    m_columns->setItem(r, 0, cell(tr("Grand total flying hours (cols 1-14)"), false, true));
    m_columns->setItem(r, 1, cell(TimeUtil::format(bfTotal, decimal, false), true, true));
    m_columns->setItem(r, 2, cell(TimeUtil::format(loggedTotal, decimal, false), true, true));
    m_columns->setItem(r, 3, cell(TimeUtil::format(bfTotal + loggedTotal, decimal, false), true, true));
    ++r;
    const int bfAero = aeronauticalMinutes(bf), loggedAero = aeronauticalMinutes(logged);
    m_columns->setItem(r, 0, cell(tr("Total aeronautical experience (cols 1-14 + 16)"), false, true));
    m_columns->setItem(r, 1, cell(TimeUtil::format(bfAero, decimal, false), true, true));
    m_columns->setItem(r, 2, cell(TimeUtil::format(loggedAero, decimal, false), true, true));
    m_columns->setItem(r, 3, cell(TimeUtil::format(bfAero + loggedAero, decimal, false), true, true));
    ++r;
    const QList<QPair<QString, int>> counts = {
        {tr("Day landings"), dayLdg}, {tr("Night landings"), nightLdg}, {tr("Instrument approaches"), apps}};
    for (const auto& [label, n] : counts) {
        m_columns->setItem(r, 0, cell(label));
        m_columns->setItem(r, 1, cell(QString(), true));
        m_columns->setItem(r, 2, cell(QString::number(n), true));
        m_columns->setItem(r, 3, cell(QString::number(n), true, true));
        ++r;
    }

    // ---- Period summaries ----
    const QDate today = QDate::currentDate();
    struct Period { QString name; QDate from; };
    const QList<Period> periods = {
        {tr("Last 30 days"), today.addDays(-30)},
        {tr("Last 90 days"), today.addDays(-90)},
        {tr("Last 6 months"), today.addMonths(-6)},
        {tr("Last 12 months"), today.addMonths(-12)},
        {tr("All (this logbook)"), QDate()},
    };
    m_periods->setRowCount(int(periods.size()));
    r = 0;
    for (const Period& p : periods) {
        int total = 0, night = 0, inst = 0, ldg = 0;
        for (const Flight& f : flights) {
            if (p.from.isValid() && (f.date <= p.from || f.date > today))
                continue;
            total += f.flightTime();
            for (const TimeColumn& c : cols)
                if (c.night)
                    night += f.value(c.key);
            inst += f.value(kInstFlightKey);
            ldg += f.dayLandings + f.nightLandings;
        }
        m_periods->setItem(r, 0, cell(p.name));
        m_periods->setItem(r, 1, cell(TimeUtil::format(total, decimal, false), true, true));
        m_periods->setItem(r, 2, cell(TimeUtil::format(night, decimal, false), true));
        m_periods->setItem(r, 3, cell(TimeUtil::format(inst, decimal, false), true));
        m_periods->setItem(r, 4, cell(QString::number(ldg), true));
        ++r;
    }
}
