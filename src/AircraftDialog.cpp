#include "AircraftDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

AircraftDialog::AircraftDialog(Database& db, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(tr("Aircraft"));
    resize(520, 400);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("Registration"), tr("Type"), tr("Engine")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { editSelected(); });

    auto* add = new QPushButton(tr("Add…"));
    auto* edit = new QPushButton(tr("Edit…"));
    auto* del = new QPushButton(tr("Delete"));
    auto* close = new QPushButton(tr("Close"));
    connect(add, &QPushButton::clicked, this, &AircraftDialog::addAircraft);
    connect(edit, &QPushButton::clicked, this, &AircraftDialog::editSelected);
    connect(del, &QPushButton::clicked, this, &AircraftDialog::deleteSelected);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(del);
    buttons->addStretch();
    buttons->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addLayout(buttons);

    reload();
}

void AircraftDialog::reload()
{
    m_list = m_db.aircraft();
    m_table->setRowCount(int(m_list.size()));
    for (int r = 0; r < m_list.size(); ++r) {
        const Aircraft& a = m_list.at(r);
        m_table->setItem(r, 0, new QTableWidgetItem(a.registration));
        m_table->setItem(r, 1, new QTableWidgetItem(a.type));
        m_table->setItem(r, 2, new QTableWidgetItem(a.engine == QLatin1String("ME") ? tr("Multi") : tr("Single")));
    }
}

qint64 AircraftDialog::selectedId() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_list.size() ? m_list.at(row).id : 0;
}

void AircraftDialog::addAircraft()
{
    Aircraft a;
    if (editAircraft(m_db, a, this))
        reload();
}

void AircraftDialog::editSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_list.size())
        return;
    Aircraft a = m_list.at(row);
    if (editAircraft(m_db, a, this))
        reload();
}

void AircraftDialog::deleteSelected()
{
    const qint64 id = selectedId();
    if (!id)
        return;
    const int used = m_db.flightCountForAircraft(id);
    if (used > 0) {
        QMessageBox::warning(this, tr("Aircraft in use"),
                             tr("This aircraft is used by %n flight(s) and can't be deleted.", nullptr, used));
        return;
    }
    if (QMessageBox::question(this, tr("Delete aircraft"), tr("Delete this aircraft?")) != QMessageBox::Yes)
        return;
    if (!m_db.deleteAircraft(id))
        QMessageBox::critical(this, tr("Error"), m_db.lastError());
    reload();
}

bool AircraftDialog::editAircraft(Database& db, Aircraft& a, QWidget* parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(a.id ? QObject::tr("Edit Aircraft") : QObject::tr("Add Aircraft"));

    auto* rego = new QLineEdit(a.registration);
    rego->setPlaceholderText(QStringLiteral("VH-ABC"));

    // Type is an editable dropdown pre-filled with types you've already entered.
    auto* type = new QComboBox;
    type->setEditable(true);
    QSet<QString> seen;
    for (const Aircraft& existing : db.aircraft()) {
        if (!seen.contains(existing.type.toUpper())) {
            seen.insert(existing.type.toUpper());
            type->addItem(existing.type);
        }
    }
    type->setCurrentText(a.type);
    type->lineEdit()->setPlaceholderText(QStringLiteral("C172"));

    auto* engine = new QComboBox;
    engine->addItem(QObject::tr("Single-engine"), QStringLiteral("SE"));
    engine->addItem(QObject::tr("Multi-engine"), QStringLiteral("ME"));
    engine->setCurrentIndex(a.engine == QLatin1String("ME") ? 1 : 0);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, [&] {
        Aircraft updated = a;
        updated.registration = rego->text().trimmed().toUpper();
        updated.type = type->currentText().trimmed().toUpper();
        updated.engine = engine->currentData().toString();
        if (updated.registration.isEmpty() || updated.type.isEmpty()) {
            QMessageBox::warning(&dlg, QObject::tr("Missing details"),
                                 QObject::tr("Enter both a registration and an aircraft type."));
            return;
        }
        if (!db.saveAircraft(updated)) {
            QMessageBox::critical(&dlg, QObject::tr("Error"), db.lastError());
            return;
        }
        a = updated;
        dlg.accept();
    });

    auto* form = new QFormLayout(&dlg);
    form->addRow(QObject::tr("Registration"), rego);
    form->addRow(QObject::tr("Type"), type);
    form->addRow(QObject::tr("Engine"), engine);
    form->addRow(box);
    return dlg.exec() == QDialog::Accepted;
}
