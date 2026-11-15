#include "RecencyWidget.h"

#include "Recency.h"
#include "Theme.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

RecencyWidget::RecencyWidget(QWidget* parent) : QWidget(parent)
{
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({tr("Item"), tr("Status"), tr("Valid until"), tr("Requirement"), tr("Detail")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);

    auto* note = new QLabel(tr(
        "Calculated from your logged landings, approaches and checks. This is a guide only — "
        "confirm requirements (including aircraft category/class and any alternatives such as "
        "proficiency checks) against CASR Part 61 and the Part 61 Manual of Standards."));
    note->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addWidget(note);
}

void RecencyWidget::refresh(const QList<Flight>& flights)
{
    const QList<RecencyItem> items = computeRecency(flights, QDate::currentDate());
    m_table->setRowCount(int(items.size()));
    for (int r = 0; r < items.size(); ++r) {
        const RecencyItem& it = items.at(r);
        QString status;
        QColor colour;
        const bool dark = isDarkActive();
        switch (it.status) {
        case RecencyItem::Status::Current:
            status = tr("Current");
            colour = dark ? QColor(0x4c, 0xc0, 0x72) : QColor(0x1b, 0x7f, 0x3a);
            break;
        case RecencyItem::Status::Expiring:
            status = tr("Expiring soon");
            colour = dark ? QColor(0xe3, 0xa3, 0x3a) : QColor(0xa8, 0x64, 0x00);
            break;
        case RecencyItem::Status::NotCurrent:
            status = tr("Not current");
            colour = dark ? QColor(0xef, 0x6b, 0x6b) : QColor(0xb4, 0x23, 0x23);
            break;
        }
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(colour);
        QFont bold = statusItem->font();
        bold.setBold(true);
        statusItem->setFont(bold);

        m_table->setItem(r, 0, new QTableWidgetItem(it.name));
        m_table->setItem(r, 1, statusItem);
        m_table->setItem(r, 2, new QTableWidgetItem(
                                   it.validUntil.isValid() ? it.validUntil.toString(QStringLiteral("dd/MM/yyyy"))
                                                           : QStringLiteral("—")));
        m_table->setItem(r, 3, new QTableWidgetItem(it.requirement));
        m_table->setItem(r, 4, new QTableWidgetItem(it.detail));
    }
}
