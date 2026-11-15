#include "SettingsDialog.h"

#include "Columns.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const Values& values, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(460);

    // ---- Appearance ----
    m_theme = new QComboBox;
    for (Theme t : {Theme::System, Theme::Light, Theme::Dark})
        m_theme->addItem(themeName(t), int(t));
    m_theme->setCurrentIndex(m_theme->findData(int(values.theme)));
    // Preview the theme as soon as it's picked; Cancel restores the previous one.
    connect(m_theme, &QComboBox::currentIndexChanged, this,
            [this] { applyTheme(Theme(m_theme->currentData().toInt())); });

    auto* appearance = new QGroupBox(tr("Appearance"));
    auto* appearanceForm = new QFormLayout(appearance);
    appearanceForm->addRow(tr("Theme"), m_theme);

    // ---- Logbook ----
    m_timeFormat = new QComboBox;
    m_timeFormat->addItem(tr("Decimal hours (1.3)"), true);
    m_timeFormat->addItem(tr("Hours and minutes (1:18)"), false);
    m_timeFormat->setCurrentIndex(values.decimal ? 0 : 1);

    m_rows = new QSpinBox;
    m_rows->setRange(5, 60);
    m_rows->setValue(values.rowsPerPage);
    m_rows->setSuffix(tr(" lines"));

    auto* logbook = new QGroupBox(tr("Logbook"));
    auto* logbookForm = new QFormLayout(logbook);
    logbookForm->addRow(tr("Show times as"), m_timeFormat);
    logbookForm->addRow(tr("Lines per page"), m_rows);

    // ---- Specialist / Instructor column names ----
    auto* spec = new QGroupBox(tr("Specialist / Instructor columns"));
    auto* specForm = new QFormLayout(spec);
    auto* hint = new QLabel(tr("Name these as headed in your logbook (e.g. Instructing, Aerobatics). "
                               "Leave blank for the default."));
    hint->setWordWrap(true);
    specForm->addRow(hint);
    for (const TimeColumn& c : timeColumns()) {
        if (!c.renamable)
            continue;
        auto* e = new QLineEdit(values.specLabels.value(c.key));
        e->setPlaceholderText(c.label);
        e->setMaxLength(24);
        m_specEdits.insert(c.key, e);
        specForm->addRow(c.label, e);
    }

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    const Theme original = values.theme;
    connect(this, &QDialog::rejected, this, [original] { applyTheme(original); });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(appearance);
    layout->addWidget(logbook);
    layout->addWidget(spec);
    layout->addWidget(box);
}

SettingsDialog::Values SettingsDialog::values() const
{
    Values v;
    v.theme = Theme(m_theme->currentData().toInt());
    v.decimal = m_timeFormat->currentData().toBool();
    v.rowsPerPage = m_rows->value();
    for (auto it = m_specEdits.cbegin(); it != m_specEdits.cend(); ++it)
        v.specLabels.insert(it.key(), it.value()->text().trimmed());
    return v;
}
