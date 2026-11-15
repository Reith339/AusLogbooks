#pragma once
#include "Theme.h"

#include <QDialog>
#include <QMap>

class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    struct Values {
        Theme theme = Theme::System;
        bool decimal = true;
        int rowsPerPage = 21;
        QMap<QString, QString> specLabels;  // column key -> label ("" = default)
    };

    explicit SettingsDialog(const Values& values, QWidget* parent = nullptr);
    Values values() const;

private:
    QComboBox* m_theme;
    QComboBox* m_timeFormat;
    QSpinBox* m_rows;
    QMap<QString, QLineEdit*> m_specEdits;
};
