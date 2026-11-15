#pragma once
#include "Database.h"

#include <QWidget>

class QTableWidget;

class TotalsWidget : public QWidget {
    Q_OBJECT
public:
    explicit TotalsWidget(QWidget* parent = nullptr);
    void refresh(const QList<Flight>& flights, const QMap<QString, int>& broughtForward, bool decimal);

private:
    QTableWidget* m_columns;
    QTableWidget* m_periods;
};
