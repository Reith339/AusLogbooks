#pragma once
#include "Database.h"

#include <QHash>
#include <QWidget>

class QLabel;
class QSpinBox;
class QTableWidget;

// Shows the logbook one page at a time, laid out like the paper CASA logbook:
// N flight lines, then Totals This Page / Brought Fwd / Totals To Date,
// plus the footer totals (flying this page, grand total, aeronautical experience).
class PageWidget : public QWidget {
    Q_OBJECT
public:
    explicit PageWidget(QWidget* parent = nullptr);
    void refresh(const QList<Flight>& flights, const QHash<qint64, Aircraft>& aircraft,
                 const QMap<QString, int>& broughtForward, bool decimal);

    static int rowsPerPageSetting();
    void setRowsPerPage(int rows);  // also saves the setting

private:
    int pageCount() const;
    void render();

    QList<Flight> m_flights;
    QHash<qint64, Aircraft> m_aircraft;
    QMap<QString, int> m_bf;
    bool m_decimal = true;
    int m_page = -1;  // 0-based; -1 = not yet shown

    QSpinBox* m_pageSpin;
    QLabel* m_pageCount;
    QLabel* m_year;
    int m_rowsPerPage = 21;
    QTableWidget* m_table;
    QLabel* m_footer;
};
