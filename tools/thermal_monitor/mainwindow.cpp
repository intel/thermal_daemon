/*
 * Thermal Monitor displays current temperature readings on a graph
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 3 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pollingdialog.h"
#include "sensorsdialog.h"
#include "logdialog.h"

#define DEFAULT_SCREEN_POS_DIVISOR 4
#define DEFAULT_SCREEN_SIZE_DIVISOR 2
#define DEFAULT_SCREEN_ASPECT_RATIO_NUM 3
#define DEFAULT_SCREEN_ASPECT_RATIO_DEN 4
#define DEFAULT_TEMP_POLL_INTERVAL_MS 4000
#define SAMPLE_STORE_SIZE 100
#define DEFAULT_LOGFILE_NAME "log.txt"
#define CUSTOMPLOT_YAXIS_RANGE 120
#define VERSION_NUMBER "1.0"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    temp_samples(SAMPLE_STORE_SIZE),
    currentTempsensorIndex(0),
    temp_poll_interval(DEFAULT_TEMP_POLL_INTERVAL_MS),
    logging_enabled(false),
    log_visible_only(false)
{
    int ret;

    ret = thermaldInterface.initialize();
    if (ret < 0) {
        sensor_name = NULL;
        sensor_visibility = NULL;
        sensor_label = NULL;
        sensor_temp = NULL;
        window = NULL;
        layout = NULL;

        QMessageBox msgBox;
        QString str;

        str = QString("Can't establish link with thermal daemon."
                      " Make sure that thermal daemon started with --dbus-enable option.\n");
        msgBox.setText(str);
        msgBox.setStandardButtons(QMessageBox::Abort);
        int ret = msgBox.exec();

        switch (ret) {
        case QMessageBox::Abort:
            // Abort was clicked
            abort();
            break;
        default:
            // should never be reached
            qFatal("main: unexpected button result");
            break;
        }
        delete ui;
        return;
    }

    // Build up a color vector for holding a good variety
    colors.append(Qt::red);
    colors.append(Qt::green);
    colors.append(Qt::blue);
    colors.append(Qt::cyan);
    colors.append(Qt::magenta);
    colors.append(Qt::yellow);
    colors.append(Qt::black);
    colors.append(QColor(200,200,00));
    colors.append(QColor(220,20,60));
    colors.append(QColor(255,20,147));
    colors.append(QColor(145,44,238));
    colors.append(QColor(0,255,127));
    colors.append(QColor(127,255,0));
    colors.append(QColor(255,215,0));
    colors.append(QColor(255,193,37));
    colors.append(QColor(255,153,18));
    colors.append(QColor(255,69,0));
    colors.append(Qt::gray);

    ui->setupUi(this);
    resoreSettings();

    for (int i = 0; i < SAMPLE_STORE_SIZE; ++i){
        temp_samples[i] = i;
    }
    displayTemperature(ui->customPlot);

    sensor_name = new QString[thermaldInterface.getSensorCount()];
    sensor_visibility = new bool[thermaldInterface.getSensorCount()];
    sensor_label = new QLabel[thermaldInterface.getSensorCount()];
    sensor_temp = new QLabel[thermaldInterface.getSensorCount()];

    for (uint i = 0; i < thermaldInterface.getSensorCount(); ++i) {
        sensorInformationType info;
        QString name;

        sensor_visibility[i] = true;
        name = thermaldInterface.getSensorName(i);
        if (!name.isEmpty()){
            addNewTemperatureSensor(ui->customPlot, name);
            sensor_name[i] = name;
        } else {
            addNewTemperatureSensor(ui->customPlot, "UNKN");
            sensor_name[i] = QString("UNKN");
        }
    }

    window = new QWidget;
    layout = new QVBoxLayout;

    layout->addWidget(ui->customPlot, 90); // 90% to this widget, 10% to tab widget
    window->setLayout(layout);
    setCentralWidget(window);
}

MainWindow::~MainWindow()
{
    if (logging_enabled){
        logging_file.close();
    }
    delete[] sensor_name;
    delete[] sensor_visibility;
    delete[] sensor_label;
    delete[] sensor_temp;
    delete window;
    delete layout;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    storeSettings();
    event->accept();
}


void MainWindow::displayTemperature(QCustomPlot *customPlot)
{
    QPen pen;

    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    // give the axes some labels:
    customPlot->xAxis->setLabel("Samples");
    customPlot->yAxis->setLabel("Temperature");
    // set axes ranges, so we see all data:
    customPlot->xAxis->setRange(0, SAMPLE_STORE_SIZE);
    customPlot->yAxis->setRange(0, CUSTOMPLOT_YAXIS_RANGE);
    customPlot->legend->setVisible(true);

    // Draw a dashed horz line for each min valid trip temperature
    for (uint zone = 0; zone < thermaldInterface.getZoneCount(); zone++){
        QCPItemLine *line = new QCPItemLine(customPlot);
        customPlot->addItem(line);
        int temp = thermaldInterface.getLowestValidTripTempForZone(zone);
        line->start->setCoords(0, temp);
        line->end->setCoords(SAMPLE_STORE_SIZE, temp);
        pen.setColor(colors[zone % colors.count()]);
        line->setPen(pen);
        trips.append(line);
    }
    connect(&tempUpdateTimer, SIGNAL(timeout()),
            this, SLOT(updateTemperatureDataSlot()));
    tempUpdateTimer.start(temp_poll_interval);
}

void MainWindow::updateTemperatureDataSlot()
{
    for (uint i = 0; i < thermaldInterface.getSensorCount(); ++i) {
        int temperature = thermaldInterface.getSensorTemperature(i);
        sensor_temp[i].setNum(temperature/1000);
        addNewTemperatureTemperatureSample(i, (double)temperature/1000.0);
    }
    if(logging_enabled){
        outStreamLogging << endl;
    }

    ui->customPlot->replot();

    // Show any active cooling devices on the status bar
    QString str;
    for (uint i = 0; i < thermaldInterface.getCoolingDeviceCount(); i++){
        int current = thermaldInterface.getCoolingDeviceCurrentState(i);
        int min = thermaldInterface.getCoolingDeviceMinState(i);
        if (current > min){
            if (str.isEmpty()){
                str += "Cooling: ";
            } else {
                str += ", ";
            }
            str += QString("%2%3 (%4/%5)")
                          .arg(thermaldInterface.getCoolingDeviceName(i))
                          .arg(i)
                          .arg(current)
                          .arg(thermaldInterface.getCoolingDeviceMaxState(i));
        }
    }
    ui->statusBar->showMessage(str);
}

int MainWindow::addNewTemperatureTemperatureSample(int index, double temperature)
{
    if (temperature_samples[index].size() >= SAMPLE_STORE_SIZE) {
        if (!temperature_samples[index].isEmpty()){
            temperature_samples[index].pop_front();
        }
        temperature_samples[index].push_back(temperature);
    } else {
        temperature_samples[index].push_back(temperature);
        current_sample_index[index]++;
    }
    ui->customPlot->graph(index)->setData(temp_samples, temperature_samples[index]);

    if (logging_enabled) {
        if (ui->customPlot->graph(index)->visible() || !log_visible_only){
            outStreamLogging << temperature << ", ";
        }
    }
    return 0;
}

int MainWindow::addNewTemperatureSensor(QCustomPlot *customPlot, QString name)
{
    static int pen_index;

    customPlot->addGraph();
    customPlot->graph(currentTempsensorIndex)->setName(name);
    customPlot->graph(currentTempsensorIndex)->setPen(QPen(colors[pen_index]));
    current_sample_index[currentTempsensorIndex] = 0;
    ++currentTempsensorIndex;
    pen_index = (pen_index + 1) % colors.count();
    return currentTempsensorIndex;
}

void MainWindow::resoreSettings()
{
    QSettings settings;

    // restore the previous window geometry, if available
    if (settings.contains("mainWindowGeometry/size")){
        resize(settings.value("mainWindowGeometry/size").toSize());
        move(settings.value("mainWindowGeometry/pos").toPoint());
    } else {
        // otherwise start with the default geometry calculated from the desktop size
        QDesktopWidget desktop;
        QRect screen = desktop.screenGeometry();
        int width, height, x, y;

        width = screen.width() / DEFAULT_SCREEN_SIZE_DIVISOR;
        height = ((screen.width() / DEFAULT_SCREEN_SIZE_DIVISOR) *
                  DEFAULT_SCREEN_ASPECT_RATIO_NUM) / DEFAULT_SCREEN_ASPECT_RATIO_DEN;
        x = screen.width() / DEFAULT_SCREEN_POS_DIVISOR;
        y = screen.height() / DEFAULT_SCREEN_POS_DIVISOR;
        resize(width, height);
        move(x, y);
    }

    // restore the previous window state, if available
    if (settings.contains("mainWindowState")){
        restoreState(settings.value("mainWindowState").toByteArray());
    }

    // restore the polling interval, if available
    if (settings.contains("updateInterval")){
        temp_poll_interval = settings.value("updateInterval").toUInt();
    } else {
        // otherwise load the default
        temp_poll_interval = DEFAULT_TEMP_POLL_INTERVAL_MS;
    }

    // restore the log file name, if available
    if (settings.contains("logFilename") &&
            !settings.value("logFilename").toString().isEmpty()){
        log_filename = settings.value("logFilename").toString();
    } else {
        // otherwise load the default
        log_filename = QString(DEFAULT_LOGFILE_NAME);
    }

    // restore the log visible only setting, if available
    if (settings.contains("logVisibleOnly")){
        log_visible_only = settings.value("logVisibleOnly").toBool();
    } else {
        // otherwise load the default
        log_visible_only = false;
    }

    // start out with logging disabled
    logging_enabled = false;
}

void MainWindow::storeSettings()
{
    QSettings settings;

    settings.setValue("mainWindowGeometry/size", size());
    settings.setValue("mainWindowGeometry/pos", pos());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("updateInterval", temp_poll_interval);
    if (!log_filename.isEmpty()){
        settings.setValue("logFilename", log_filename);
    } else {
        // restore default log file name from empty string name
        settings.setValue("logFilename", QString(DEFAULT_LOGFILE_NAME));
    }
    settings.setValue("logVisibleOnly", log_visible_only);
}

void MainWindow::changePollIntervalSlot(uint new_val)
{
    temp_poll_interval = new_val;
    tempUpdateTimer.start(temp_poll_interval);
}

void MainWindow::changeGraphVisibilitySlot(uint index, bool visible){
    if (index < (uint)thermaldInterface.getSensorCount()){
        ui->customPlot->graph(index)->setVisible(visible);
        sensor_visibility[index] = visible;
    }
}

void MainWindow::changeLogVariables(bool log_enabled, bool log_vis_only,
                                    QString log_file_name)
{
    if (log_enabled && log_file_name.isEmpty()){
        QMessageBox msgBox(this);
        msgBox.setText("Please enter a valid filename.\r\rLogging not enabled");
        msgBox.exec();
        return;
    }

    /* if logging has just been turned on, open the file to write,
     * then output the sensor name header
     */
    if (!logging_enabled && log_enabled){
        // first set the file and output stream
        logging_file.setFileName(log_file_name);
        QTextStream out(&logging_file);
        if (!logging_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "Cannot open file for writing: "
                        << qPrintable(logging_file.errorString()) << endl;
            return;
        }
        outStreamLogging.setDevice(&logging_file);

        // now output the temperature sensor names as a header
        for(uint i = 0; i < thermaldInterface.getSensorCount(); i++){
            if (!log_vis_only || ui->customPlot->graph(i)->visible()){
                outStreamLogging << ui->customPlot->graph(i)->name() << ", ";
            }
        }
        outStreamLogging << endl;
    }

    // logging has been turned off, so close the file
    if (logging_enabled && !log_enabled){
        logging_file.close();
    }

    // copy the flags and filename to this
    logging_enabled = log_enabled;
    log_visible_only = log_vis_only;
    log_filename = log_file_name;
}

void MainWindow::currentChangedSlot(int index)
{
    if (index) {
        ui->customPlot->setVisible(false);
    } else {
        ui->customPlot->setVisible(true);
    }
}

void MainWindow::on_actionClear_triggered()
{
    QSettings settings;
    settings.clear();
    QCoreApplication::quit();
}

void MainWindow::on_actionSet_Polling_Interval_triggered()
{
    PollingDialog *p = new PollingDialog(this, temp_poll_interval);

    QObject::connect(p, SIGNAL(setPollInterval(uint)),
                     this, SLOT(changePollIntervalSlot(uint)));
    p->show(); // non-modal
}

void MainWindow::on_actionSensors_triggered()
{
    SensorsDialog *s = new SensorsDialog(this);

    QObject::connect(s, SIGNAL(setGraphVisibility(uint, bool)),
                     this, SLOT(changeGraphVisibilitySlot(uint, bool)));

    // set the checkbox names to match the temperature sensor names
    for (uint i = 0; i < MAX_NUMBER_SENSOR_VISIBILITY_CHECKBOXES; i++){
        if (i < thermaldInterface.getSensorCount()){
            s->setupCheckbox(i, sensor_name[i], sensor_visibility[i]);
        } else {
            s->disableCheckbox(i);
        }
    }

    // future project: create checkboxes via code, not .ui
    s->show();
}

void MainWindow::on_actionLog_triggered()
{
    LogDialog *l = new LogDialog(this);

    QObject::connect(l, SIGNAL(setLogVariable(bool, bool, QString)),
                     this, SLOT(changeLogVariables(bool, bool, QString)));
    l->setLoggingState(logging_enabled, log_visible_only, log_filename);
    l->show();
}

void MainWindow::on_action_About_triggered()
{
    QMessageBox msgBox;
    QString str;

    str = QString("<p align='center'>Thermal Monitor<br><br>"
                  "Version %1<br><br>"
                  "GUI for Linux thermal daemon (thermald)<br><br>"
                  "Copyright (c) 2015, Intel Corporation</p>")
            .arg(QString(VERSION_NUMBER));
    msgBox.setText(str);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setWindowTitle(QString("About Thermal Monitor"));
    msgBox.exec();
}


void MainWindow::on_actionE_xit_triggered()
{
    close();
}
