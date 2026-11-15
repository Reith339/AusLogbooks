#include "PageWidget.h"

#include "Columns.h"
#include "TimeUtil.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
enum FixedCol { Month, Day, Type, Reg, Pic, Crew, Details, FixedCount };
constexpr int kTotalsRows = 3;
const QString kRowsSetting = QStringLiteral("display/rowsPerPage");

QMap<QString, int> sum(const QList<Flight>& flights, int from, int to)
{
    QMap<QString, int> m;
    for (int i = from; i < to && i < flights.size(); ++i)
        for (auto it = flights[i].minutes.cbegin(); it != flights[i].minutes.cend(); ++it)
            m[it.key()] += it.value();
    return m;
}

QMap<QString, int> add(QMap<QString, int> a, const QMap<QString, int>& b)
{
    for (auto it = b.cbegin(); it != b.cend(); ++it)
        a[it.key()] += it.value();
    return a;
}
}  // namespace

PageWidget::PageWidget(QWidget* parent) : QWidget(parent)
{
    auto* prev = new QPushButton(QStringLiteral("◀"));
    auto* next = new QPushButton(QStringLiteral("▶"));
    prev->setFixedWidth(36);
    next->setFixedWidth(36);
    m_pageSpin = new QSpinBox;
    m_pageSpin->setMinimum(1);
    m_pageCount = new QLabel;
    m_year = new QLabel;
    QFont big = m_year->font();
    big.setBold(true);
    big.setPointSize(big.pointSize() + 2);
    m_year->setFont(big);
    m_rowsPerPage = rowsPerPageSetting();

    connect(prev, &QPushButton::clicked, this, [this] { m_pageSpin->setValue(m_pageSpin->value() - 1); });
    connect(next, &QPushButton::clicked, this, [this] { m_pageSpin->setValue(m_pageSpin->value() + 1); });
    connect(m_pageSpin, &QSpinBox::valueChanged, this, [this](int v) {
        if (v - 1 != m_page) {
            m_page = v - 1;
            render();
        }
    });

    auto* bar = new QHBoxLayout;
    bar->addWidget(m_year);
    bar->addStretch();
    bar->addWidget(new QLabel(tr("Page")));
    bar->addWidget(prev);
    bar->addWidget(m_pageSpin);
    bar->addWidget(next);
    bar->addWidget(m_pageCount);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setWordWrap(false);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    m_footer = new QLabel;
    m_footer->setTextFormat(Qt::RichText);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_footer);
}

int PageWidget::rowsPerPageSetting()
{
    return qBound(5, QSettings().value(kRowsSetting, 21).toInt(), 60);
}

void PageWidget::setRowsPerPage(int rows)
{
    rows = qBound(5, rows, 60);
    QSettings().setValue(kRowsSetting, rows);
    if (rows == m_rowsPerPage)
        return;
    m_rowsPerPage = rows;
    m_page = pageCount() - 1;
    render();
}

int PageWidget::pageCount() const
{
    const int rows = m_rowsPerPage;
    return qMax(1, int((m_flights.size() + rows - 1) / rows));
}

void PageWidget::refresh(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft,
                         const QMap<QString, int>& broughtForward, bool decimal)
{
    const bool wasOnLastPage = m_page < 0 || m_page >= pageCount() - 1;
    m_flights = flights;
    m_aircraft = aircraft;
    m_bf = broughtForward;
    m_decimal = decimal;
    if (wasOnLastPage)
        m_page = pageCount() - 1;
    render();
}

void PageWidget::render()
{
    const auto& cols = timeColumns();
    const int rowsPerPage = m_rowsPerPage;
    const int pages = pageCount();
    m_page = qBound(0, m_page, pages - 1);

    {
        const QSignalBlocker block(m_pageSpin);
        m_pageSpin->setMaximum(pages);
        m_pageSpin->setValue(m_page + 1);
    }
    m_pageCount->setText(tr("of %1").arg(pages));

    const int first = m_page * rowsPerPage;
    const int last = qMin(first + rowsPerPage, int(m_flights.size()));

    // Year heading (pages can span a new year).
    int y0 = 0, y1 = 0;
    for (int i = first; i < last; ++i) {
        const int y = m_flights[i].date.year();
        if (!y0) y0 = y;
        y1 = y;
    }
    m_year->setText(!y0 ? tr("Year —") : y0 == y1 ? tr("Year %1").arg(y0) : tr("Year %1–%2").arg(y0).arg(y1));

    // ---- Headers ----
    QStringList headers = {tr("Month"), tr("Day"), tr("Aircraft\nType"), tr("Aircraft\nReg."),
                           tr("Pilot in\nCommand"), tr("Other Pilot\nor Crew"), tr("Details")};
    for (const TimeColumn& c : cols)
        headers << columnHeader(c, QStringLiteral("\n"));
    m_table->clear();
    m_table->clearSpans();
    m_table->setColumnCount(int(headers.size()));
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(rowsPerPage + kTotalsRows);

    // Night columns are shaded, as in the paper logbook (works in light and dark themes).
    const QColor base = QApplication::palette().color(QPalette::Base);
    const QColor shade = base.lightness() < 128 ? base.lighter(150) : base.darker(108);
    auto timeItem = [&](int minutes, const TimeColumn& c, bool bold) {
        auto* item = new QTableWidgetItem(TimeUtil::format(minutes, m_decimal));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (c.night)
            item->setBackground(shade);
        if (bold) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        return item;
    };

    // ---- Flight lines ----
    QStringList rowLabels;
    for (int r = 0; r < rowsPerPage; ++r) {
        rowLabels << QString::number(r + 1);
        const int i = first + r;
        if (i >= last) {
            for (int c = 0; c < cols.size(); ++c)
                m_table->setItem(r, FixedCount + c, timeItem(0, cols[c], false));
            continue;
        }
        const Flight& f = m_flights[i];
        const Aircraft ac = m_aircraft.value(f.aircraftId);
        QString details = f.details;
        if (!f.dep.isEmpty() || !f.arr.isEmpty()) {
            const QString route = f.dep == f.arr || f.arr.isEmpty() ? f.dep
                                : f.dep.isEmpty()                    ? f.arr
                                                                     : f.dep + QStringLiteral("–") + f.arr;
            details = details.isEmpty() ? route : route + QStringLiteral("  ") + details;
        }
        m_table->setItem(r, Month, new QTableWidgetItem(f.date.toString(QStringLiteral("MMM")).toUpper()));
        m_table->setItem(r, Day, new QTableWidgetItem(QString::number(f.date.day())));
        m_table->setItem(r, Type, new QTableWidgetItem(f.aircraftId ? ac.type : QStringLiteral("SIM")));
        m_table->setItem(r, Reg, new QTableWidgetItem(ac.registration));
        m_table->setItem(r, Pic, new QTableWidgetItem(f.pic));
        m_table->setItem(r, Crew, new QTableWidgetItem(f.copilot));
        m_table->setItem(r, Details, new QTableWidgetItem(details));
        for (int c = 0; c < cols.size(); ++c)
            m_table->setItem(r, FixedCount + c, timeItem(f.value(cols[c].key), cols[c], false));
    }

    // ---- Totals rows ----
    const QMap<QString, int> thisPage = sum(m_flights, first, last);
    const QMap<QString, int> bfwd = add(m_bf, sum(m_flights, 0, first));
    const QMap<QString, int> toDate = add(bfwd, thisPage);
    const QList<QPair<QString, QMap<QString, int>>> totals = {
        {tr("TOTALS THIS PAGE"), thisPage},
        {tr("TOTALS BROUGHT FWD FROM PREVIOUS PAGE"), bfwd},
        {tr("TOTALS TO DATE"), toDate},
    };
    int r = rowsPerPage;
    for (const auto& [label, values] : totals) {
        rowLabels << QString();
        auto* l = new QTableWidgetItem(label);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        l->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 0, l);
        m_table->setSpan(r, 0, 1, FixedCount);
        for (int c = 0; c < cols.size(); ++c) {
            const int m = values.value(cols[c].key);
            auto* item = timeItem(m, cols[c], true);
            if (m == 0)
                item->setText(TimeUtil::format(0, m_decimal, false));
            m_table->setItem(r, FixedCount + c, item);
        }
        ++r;
    }
    m_table->setVerticalHeaderLabels(rowLabels);
    m_table->resizeColumnsToContents();
    m_table->setColumnWidth(Details, qMax(m_table->columnWidth(Details), 220));

    auto fmt = [this](int m) { return TimeUtil::format(m, m_decimal, false); };
    m_footer->setText(
        tr("Total flying this page: <b>%1</b> hrs &nbsp;&nbsp;·&nbsp;&nbsp; "
           "Grand total flying hours: <b>%2</b> hrs &nbsp;&nbsp;·&nbsp;&nbsp; "
           "Total aeronautical experience: <b>%3</b> hrs "
           "<span style='color:gray'>(flying = cols 1-14, experience = cols 1-14 + 16)</span>")
            .arg(fmt(flightMinutes(thisPage)), fmt(flightMinutes(toDate)), fmt(aeronauticalMinutes(toDate))));
}
