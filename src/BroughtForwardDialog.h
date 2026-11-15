#pragma once
#include <QDialog>
#include <QMap>

class QLineEdit;

// Totals carried over from a previous (paper) logbook, per column.
class BroughtForwardDialog : public QDialog {
    Q_OBJECT
public:
    BroughtForwardDialog(const QMap<QString, int>& values, bool decimal, QWidget* parent = nullptr);
    QMap<QString, int> values() const { return m_values; }

protected:
    void accept() override;

private:
    QMap<QString, QLineEdit*> m_edits;
    QMap<QString, int> m_values;
};
