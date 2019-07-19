#ifndef TRIPSDIALOG_H
#define TRIPSDIALOG_H

#include <QDialog>
#include "thermaldinterface.h"
#include "ui_tripsdialog.h"

namespace Ui {
class tripsDialog;
}

class tripsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit tripsDialog(QWidget *parent = 0, ThermaldInterface *therm = 0);
    ~tripsDialog();

    void addZone(zoneInformationType *zone);

signals:
    void changeTripSetpoint(uint zone, uint trip, int temperature);
    void setTripVis(uint zone, uint trip, bool visibility);

private slots:
    void on_treeWidget_clicked(const QModelIndex &index);
    void on_treeWidget_doubleClicked(const QModelIndex &index);
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_treeWidget_expanded();
    void on_treeWidget_collapsed();
    void on_lineEdit_editingFinished();

private:
    Ui::tripsDialog *ui;
    ThermaldInterface *thermal;
    QVector<uint> zone_display_list;

    QTreeWidgetItem *last_item;
    uint last_trip;
    uint last_zone;

    void resizeColumns();
    void setRowHighlighting(QTreeWidgetItem *item, bool visible);
};

#endif // TRIPSDIALOG_H
