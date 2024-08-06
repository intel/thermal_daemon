#include "tripsdialog.h"
#include "ui_tripsdialog.h"

tripsDialog::tripsDialog(QWidget *parent, ThermaldInterface *therm) :
    QDialog(parent),
    ui(new Ui::tripsDialog)
{
    thermal = therm;

    ui->setupUi(this);

    ui->label->setText("");
    ui->label_2->setText("");
    ui->lineEdit->setEnabled(false);
    ui->lineEdit->setValidator(new QIntValidator);

    this->last_item = NULL;
    this->last_trip = 0;
    this->last_zone = 0;
}

tripsDialog::~tripsDialog()
{
    delete ui;
}

void tripsDialog::addZone(zoneInformationType *zone)
{
    if (!zone->active)
        return;

    QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->treeWidget);

    treeItem->setText(0, zone->name);
    for(uint i = 0; i < (uint)zone->trips.count(); i++){
        QTreeWidgetItem *treeTripItem = new QTreeWidgetItem(treeItem);
        treeTripItem->setData(1, Qt::DisplayRole, zone->trips[i].temp);
        treeTripItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        zone_display_list.append(zone->id);

        QString str;
        switch(zone->trips[i].trip_type){
        case CRITICAL_TRIP:
            str = "Critical";
            break;
        case MAX_TRIP:
            str = "Max";
            break;
        case PASSIVE_TRIP:
            str = "Passive";
            break;
        case ACTIVE_TRIP:
            str = "Active";
            break;
        case POLLING_TRIP:
            str = "Polling";
            break;
        case INVALID_TRIP:
            str = "Invalid";
            break;
        default:
            qCritical() << "tripsDialog::addZone"
                        << "invalid trip type: " << zone->trips[i].trip_type;
            str = "I'm losing my mind Dave";
        }
        treeTripItem->setText(2, str);

        setRowHighlighting(treeTripItem, zone->trips[i].visible);
    }
    // Sorting by temperature looks better, but we would have to track the trip index
    //    treeItem->sortChildren(1, Qt::AscendingOrder);

    ui->treeWidget->setToolTip("Click on temperature to change.\n"
                               "Double click to display or clear.");
    resizeColumns();
}

void tripsDialog::resizeColumns()
{
    for (int i = 0; i < ui->treeWidget->columnCount(); i++){
        ui->treeWidget->resizeColumnToContents(i);
    }
}

void tripsDialog::setRowHighlighting(QTreeWidgetItem *item, bool visible)
{
    if (visible){
        item->setForeground(1, Qt::white);
        item->setBackground(1, Qt::black);
        item->setForeground(2, Qt::white);
        item->setBackground(2, Qt::black);
        item->setForeground(3, Qt::white);
        item->setBackground(3, Qt::black);
        item->setText(3, "Yes");
    } else {
        item->setForeground(1, Qt::black);
        item->setBackground(1, Qt::white);
        item->setForeground(2, Qt::black);
        item->setBackground(2, Qt::white);
        item->setForeground(3, Qt::black);
        item->setBackground(3, Qt::white);
        item->setText(3, "");
    }
}

void tripsDialog::on_treeWidget_clicked(const QModelIndex &index)
{
    int zone;
    int trip;
    int col;

    // check to see if the user clicked on a zone root node
    if(index.parent().column() == -1){
        zone = index.row();
        col = index.column();
        trip = -1;
    } else {  // otherwise the user clicked on a trip
        zone = index.parent().row();
        zone = zone_display_list[zone];
        col = index.column();
        trip = index.row();
    }

    // Alternate the background color, black to display on the graph, white to ignore
    if (trip != -1 && col == 1){ // if the user clicks on a temperature
        ui->label->setText("Zone: " + index.parent().data().toString());

        const QAbstractItemModel *model = index.model();
        const QModelIndex &child = model->index(trip, col + 1, index.parent());

        ui->label_2->setText("Type: " + child.data().toString() + " (°C)");
        ui->lineEdit->setText(index.data().toString());

        // ACTIVE_TRIP modification not supported at this time
        if (thermal->getTripTypeForZone(zone, trip) == MAX_TRIP ||
                thermal->getTripTypeForZone(zone, trip) == PASSIVE_TRIP){
            ui->lineEdit->setEnabled(true);
            // store the zone and trip in case the user edits the trip temperature
            last_zone = zone;
            last_trip = trip;
            last_item = ui->treeWidget->currentItem();
        } else {
            ui->lineEdit->setEnabled(false);
        }
    } else {
        ui->label->setText("Click on");
        ui->label_2->setText("temperature");
        ui->lineEdit->setText("");
        ui->lineEdit->setEnabled(false);
    }
    resizeColumns();
}

void tripsDialog::on_treeWidget_doubleClicked(const QModelIndex &index)
{
    int zone;
    int trip;
    // check to see if the user clicked on a zone root node
    if(index.parent().column() == -1){
        zone = index.row();
    } else {  // otherwise the user clicked on a trip
        zone = index.parent().row();
        zone = zone_display_list[zone];
        trip = index.row();

        // Invert the fore and background colors to display trip on the graph
        // Look at column 1, the trip temperature, to determine the outcome
          if (ui->treeWidget->currentItem()->foreground(1).color() == Qt::black){
            setRowHighlighting(ui->treeWidget->currentItem(), true);
            emit setTripVis(zone, trip, true);
        } else {
            setRowHighlighting(ui->treeWidget->currentItem(), false);
            emit setTripVis(zone, trip, false);

        }
    }
    resizeColumns();
}

void tripsDialog::on_buttonBox_accepted()
{

}

void tripsDialog::on_buttonBox_rejected()
{

}

void tripsDialog::on_treeWidget_expanded()
{
    resizeColumns();
}

void tripsDialog::on_treeWidget_collapsed()
{
    resizeColumns();
}

void tripsDialog::on_lineEdit_editingFinished()
{
    last_item->setText(1, ui->lineEdit->text());
    if (ui->lineEdit->text().toInt() != thermal->getTripTempForZone(last_zone, last_trip)){
        emit changeTripSetpoint(last_zone, last_trip, ui->lineEdit->text().toInt());
    }
}
