#include "BroughtForwardDialog.h"

#include "Columns.h"
#include "TimeUtil.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

BroughtForwardDialog::BroughtForwardDialog(const QMap<QString, int>& values, bool decimal, QWidget* parent)
    : QDialog(parent), m_values(values)
{
    setWindowTitle(tr("Brought Forward"));

    auto* intro = new QLabel(tr("Enter the totals from your previous logbook. They're added to "
                                "the running total and the totals page."));
    intro->setWordWrap(true);

    auto* grid = new QGridLayout;
    QMap<QString, QFormLayout*> forms;
    for (const TimeColumn& c : timeColumns()) {
        if (!forms.contains(c.group)) {
            auto* g = new QGroupBox(c.group);
            auto* form = new QFormLayout(g);
            const int n = int(forms.size());
            forms.insert(c.group, form);
            grid->addWidget(g, n / 3, n % 3);
        }
        const QString label = c.number ? QStringLiteral("%1 (%2)").arg(columnLabel(c)).arg(c.number)
                                       : columnLabel(c);
        auto* e = new QLineEdit(TimeUtil::format(values.value(c.key), decimal));
        e->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral(R"(^\d{0,5}([.:]\d{0,2})?$)")), e));
        e->setMaximumWidth(80);
        e->setAlignment(Qt::AlignRight);
        m_edits.insert(c.key, e);
        forms.value(c.group)->addRow(label, e);
    }

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, this, &BroughtForwardDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(grid);
    layout->addWidget(box);
}

void BroughtForwardDialog::accept()
{
    QMap<QString, int> result;
    for (const TimeColumn& c : timeColumns()) {
        int m = 0;
        if (!TimeUtil::parse(m_edits.value(c.key)->text(), &m)) {
            QMessageBox::warning(this, tr("Invalid time"), tr("%1 — %2 isn't a valid time.").arg(c.group, columnLabel(c)));
            return;
        }
        result.insert(c.key, m);
    }
    m_values = result;
    QDialog::accept();
}
