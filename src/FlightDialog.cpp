#include "FlightDialog.h"

#include "AircraftDialog.h"
#include "Columns.h"
#include "TimeUtil.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QCompleter* makeCompleter(const QStringList& words, QObject* parent)
{
    auto* c = new QCompleter(words, parent);
    c->setCaseSensitivity(Qt::CaseInsensitive);
    c->setFilterMode(Qt::MatchStartsWith);
    return c;
}
}  // namespace

FlightDialog::FlightDialog(Database& db, const Flight& flight, bool decimal, QWidget* parent)
    : QDialog(parent), m_db(db), m_flight(flight), m_decimal(decimal)
{
    setWindowTitle(flight.id ? tr("Edit Flight") : tr("Add Flight"));

    // Suggestions from previous entries.
    QSet<QString> names, places;
    for (const Flight& f : db.flights()) {
        if (!f.pic.isEmpty()) names.insert(f.pic);
        if (!f.copilot.isEmpty()) names.insert(f.copilot);
        if (!f.dep.isEmpty()) places.insert(f.dep);
        if (!f.arr.isEmpty()) places.insert(f.arr);
    }
    QStringList nameList(names.begin(), names.end());
    QStringList placeList(places.begin(), places.end());
    nameList.sort(Qt::CaseInsensitive);
    placeList.sort(Qt::CaseInsensitive);

    // ---- Flight details ----
    m_date = new QDateEdit(flight.date.isValid() ? flight.date : QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    m_aircraft = new QComboBox;
    m_aircraft->setMinimumWidth(240);
    auto* manage = new QPushButton(tr("Manage…"));
    connect(manage, &QPushButton::clicked, this, &FlightDialog::manageAircraft);
    auto* aircraftRow = new QHBoxLayout;
    aircraftRow->addWidget(m_aircraft, 1);
    aircraftRow->addWidget(manage);

    QString defaultPic = flight.pic;
    if (!flight.id && defaultPic.isEmpty())
        defaultPic = QStringLiteral("Self");  // new flights always start as Self
    m_pic = new QLineEdit(defaultPic);
    m_pic->setCompleter(makeCompleter(nameList, this));
    m_copilot = new QLineEdit(flight.copilot);
    m_copilot->setCompleter(makeCompleter(nameList, this));
    m_dep = new QLineEdit(flight.dep);
    m_dep->setCompleter(makeCompleter(placeList, this));
    m_dep->setPlaceholderText(tr("e.g. YMMB"));
    m_arr = new QLineEdit(flight.arr);
    m_arr->setCompleter(makeCompleter(placeList, this));
    m_arr->setPlaceholderText(tr("e.g. YLTV"));
    m_details = new QLineEdit(flight.details);
    m_details->setPlaceholderText(tr("Route, exercise, remarks"));

    auto* routeRow = new QHBoxLayout;
    routeRow->addWidget(m_dep);
    routeRow->addWidget(new QLabel(QStringLiteral("→")));
    routeRow->addWidget(m_arr);

    auto* detailsBox = new QGroupBox(tr("Flight"));
    auto* detailsForm = new QFormLayout(detailsBox);
    detailsForm->addRow(tr("Date"), m_date);
    detailsForm->addRow(tr("Aircraft"), aircraftRow);
    detailsForm->addRow(tr("Pilot in command"), m_pic);
    detailsForm->addRow(tr("Other pilot or crew"), m_copilot);
    detailsForm->addRow(tr("From / To"), routeRow);
    detailsForm->addRow(tr("Details of flight"), m_details);

    // ---- Quick fill ----
    m_qTotal = makeTimeEdit(0);
    m_qNight = makeTimeEdit(0);
    m_qInst = makeTimeEdit(0);
    m_qRole = new QComboBox;
    m_qRole->addItems({Role::Command, Role::Dual, Role::ICUS, Role::CoPilot});
    auto* fill = new QPushButton(tr("Fill columns"));
    connect(fill, &QPushButton::clicked, this, &FlightDialog::quickFill);

    auto* quickBox = new QGroupBox(tr("Quick fill (uses the aircraft's engine type)"));
    auto* quick = new QHBoxLayout(quickBox);
    quick->addWidget(new QLabel(tr("Total")));
    quick->addWidget(m_qTotal);
    quick->addWidget(new QLabel(tr("of which night")));
    quick->addWidget(m_qNight);
    quick->addWidget(new QLabel(tr("Instrument")));
    quick->addWidget(m_qInst);
    quick->addWidget(new QLabel(tr("as")));
    quick->addWidget(m_qRole);
    quick->addWidget(fill);

    // ---- Time columns, grouped as in the logbook ----
    auto* timesBox = new QGroupBox(tr("Flight time"));
    auto* grid = new QGridLayout(timesBox);
    constexpr int kGroupsPerRow = 4;
    QMap<QString, QFormLayout*> groupForms;
    QStringList groupOrder;
    // Numbered logbook columns first, then the Specialist / Instructor columns.
    QList<TimeColumn> ordered;
    for (const TimeColumn& c : timeColumns())
        if (!c.renamable)
            ordered << c;
    for (const TimeColumn& c : timeColumns())
        if (c.renamable)
            ordered << c;
    for (const TimeColumn& c : ordered) {
        if (!groupForms.contains(c.group)) {
            auto* g = new QGroupBox(c.group);
            auto* form = new QFormLayout(g);
            groupForms.insert(c.group, form);
            const int n = int(groupOrder.size());
            groupOrder << c.group;
            grid->addWidget(g, n / kGroupsPerRow, n % kGroupsPerRow);
        }
        QLineEdit* e = makeTimeEdit(flight.value(c.key));
        connect(e, &QLineEdit::textChanged, this, &FlightDialog::updateTotal);
        m_timeEdits.insert(c.key, e);
        const QString label = c.number ? QStringLiteral("%1 (%2)").arg(columnLabel(c)).arg(c.number)
                                       : columnLabel(c);
        groupForms.value(c.group)->addRow(label, e);
    }

    // Landings / approaches / checks, in the next free grid cell.
    auto* otherBox = new QGroupBox(tr("Landings && Checks"));
    auto* otherForm = new QFormLayout(otherBox);
    m_dayLdg = new QSpinBox;
    m_dayLdg->setRange(0, 99);
    m_dayLdg->setValue(flight.dayLandings);
    m_nightLdg = new QSpinBox;
    m_nightLdg->setRange(0, 99);
    m_nightLdg->setValue(flight.nightLandings);
    m_apps = new QSpinBox;
    m_apps->setRange(0, 99);
    m_apps->setValue(flight.approaches);
    m_review = new QCheckBox(tr("Flight review"));
    m_review->setChecked(flight.flightReview);
    m_ipc = new QCheckBox(tr("Instrument proficiency check"));
    m_ipc->setChecked(flight.ipc);
    otherForm->addRow(tr("Day landings"), m_dayLdg);
    otherForm->addRow(tr("Night landings"), m_nightLdg);
    otherForm->addRow(tr("Instrument approaches"), m_apps);
    otherForm->addRow(m_review);
    otherForm->addRow(m_ipc);
    const int n = int(groupOrder.size());
    grid->addWidget(otherBox, n / kGroupsPerRow, n % kGroupsPerRow);

    m_totalLabel = new QLabel;
    QFont bold = m_totalLabel->font();
    bold.setBold(true);
    m_totalLabel->setFont(bold);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, this, &FlightDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(m_totalLabel);
    bottom->addStretch();
    bottom->addWidget(box);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(detailsBox);
    layout->addWidget(quickBox);
    layout->addWidget(timesBox);
    layout->addLayout(bottom);

    reloadAircraft(flight.id ? flight.aircraftId
                             : QSettings().value(QStringLiteral("entry/lastAircraft"), 0).toLongLong());
    updateTotal();
}

QLineEdit* FlightDialog::makeTimeEdit(int minutes)
{
    auto* e = new QLineEdit(TimeUtil::format(minutes, m_decimal));
    e->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral(R"(^\d{0,3}([.:]\d{0,2})?$)")), e));
    e->setPlaceholderText(m_decimal ? QStringLiteral("0.0") : QStringLiteral("0:00"));
    e->setMaximumWidth(70);
    e->setAlignment(Qt::AlignRight);
    return e;
}

void FlightDialog::reloadAircraft(qint64 selectId)
{
    m_aircraft->clear();
    m_aircraftById.clear();
    m_aircraft->addItem(tr("— none / simulator —"), 0);
    for (const Aircraft& a : m_db.aircraft()) {
        m_aircraftById.insert(a.id, a);
        m_aircraft->addItem(a.display(), a.id);
    }
    const int idx = m_aircraft->findData(selectId);
    m_aircraft->setCurrentIndex(idx >= 0 ? idx : 0);
}

void FlightDialog::manageAircraft()
{
    const qint64 current = m_aircraft->currentData().toLongLong();
    AircraftDialog dlg(m_db, this);
    dlg.exec();
    reloadAircraft(current);
}

void FlightDialog::quickFill()
{
    int total = 0, night = 0, inst = 0;
    if (!TimeUtil::parse(m_qTotal->text(), &total) || !TimeUtil::parse(m_qNight->text(), &night)
        || !TimeUtil::parse(m_qInst->text(), &inst)) {
        QMessageBox::warning(this, tr("Quick fill"), tr("Enter times like 1.3 or 1:18."));
        return;
    }
    if (night > total) {
        QMessageBox::warning(this, tr("Quick fill"), tr("Night time can't be more than the total."));
        return;
    }
    const qint64 id = m_aircraft->currentData().toLongLong();
    if (!id) {
        QMessageBox::warning(this, tr("Quick fill"), tr("Select an aircraft first."));
        return;
    }
    const auto engine = m_aircraftById.value(id).engine == QLatin1String("ME") ? TimeColumn::Engine::ME
                                                                              : TimeColumn::Engine::SE;
    const QString role = m_qRole->currentText();
    const QString dayKey = findColumnKey(engine, false, role);
    const QString nightKey = findColumnKey(engine, true, role);
    if (dayKey.isEmpty() || nightKey.isEmpty()) {
        QMessageBox::warning(this, tr("Quick fill"),
                             tr("The logbook has no %1 column for this engine type.").arg(role));
        return;
    }

    for (const TimeColumn& c : timeColumns())
        if (c.countsToFlightTime)
            m_timeEdits.value(c.key)->clear();
    if (total - night > 0)
        m_timeEdits.value(dayKey)->setText(TimeUtil::format(total - night, m_decimal));
    if (night > 0)
        m_timeEdits.value(nightKey)->setText(TimeUtil::format(night, m_decimal));
    if (!m_qInst->text().trimmed().isEmpty())
        m_timeEdits.value(kInstFlightKey)->setText(TimeUtil::format(inst, m_decimal));
}

void FlightDialog::updateTotal()
{
    QMap<QString, int> mins;
    bool ok = true;
    for (auto it = m_timeEdits.cbegin(); it != m_timeEdits.cend(); ++it) {
        int m = 0;
        if (!TimeUtil::parse(it.value()->text(), &m))
            ok = false;
        mins.insert(it.key(), m);
    }
    m_totalLabel->setText(ok ? tr("Total flying (cols 1-14): %1").arg(TimeUtil::format(flightMinutes(mins), m_decimal, false))
                             : tr("Total flying: (invalid time)"));
}

void FlightDialog::accept()
{
    Flight f = m_flight;
    f.date = m_date->date();
    f.aircraftId = m_aircraft->currentData().toLongLong();
    f.pic = m_pic->text().trimmed();
    f.copilot = m_copilot->text().trimmed();
    f.dep = m_dep->text().trimmed().toUpper();
    f.arr = m_arr->text().trimmed().toUpper();
    f.details = m_details->text().trimmed();
    f.minutes.clear();
    for (const TimeColumn& c : timeColumns()) {
        int m = 0;
        if (!TimeUtil::parse(m_timeEdits.value(c.key)->text(), &m)) {
            QMessageBox::warning(this, tr("Invalid time"),
                                 tr("%1 — %2 isn't a valid time. Use 1.3 or 1:18.").arg(c.group, columnLabel(c)));
            m_timeEdits.value(c.key)->setFocus();
            return;
        }
        if (m)
            f.minutes.insert(c.key, m);
    }
    f.dayLandings = m_dayLdg->value();
    f.nightLandings = m_nightLdg->value();
    f.approaches = m_apps->value();
    f.flightReview = m_review->isChecked();
    f.ipc = m_ipc->isChecked();

    if (f.flightTime() > 0 && !f.aircraftId) {
        QMessageBox::warning(this, tr("Missing aircraft"),
                             tr("Select an aircraft for flight time. Use \"none / simulator\" only for sim time."));
        return;
    }
    if (f.flightTime() == 0 && f.value(kSimKey) == 0) {
        QMessageBox::warning(this, tr("No time entered"), tr("Enter flight time (or simulator time)."));
        return;
    }

    QSettings settings;
    settings.remove(QStringLiteral("entry/lastPic"));  // no longer used
    settings.setValue(QStringLiteral("entry/lastAircraft"), f.aircraftId);
    m_flight = f;
    QDialog::accept();
}
