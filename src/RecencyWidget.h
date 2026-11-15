#pragma once
#include "Database.h"

#include <QWidget>

class QTableWidget;

class RecencyWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecencyWidget(QWidget* parent = nullptr);
    void refresh(const QList<Flight>& flights);

private:
    QTableWidget* m_table;
};
