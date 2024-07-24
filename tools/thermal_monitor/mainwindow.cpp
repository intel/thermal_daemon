/* * Thermal Monitor displays current temperature readings on a graph
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

#include <QMessageBox>
#include <QScreen>
#include "mainwindow.h"
#include "pollingdialog.h"
#include "sensorsdialog.h"
#include "logdialog.h"
#include "tripsdialog.h"

#define DEFAULT_SCREEN_POS_DIVISOR 4
#define DEFAULT_SCREEN_SIZE_DIVISOR 2
#define DEFAULT_SCREEN_ASPECT_RATIO_NUM 3
#define DEFAULT_SCREEN_ASPECT_RATIO_DEN 4
#define DEFAULT_TEMP_POLL_INTERVAL_MS 4000
#define SAMPLE_STORE_SIZE 100
#define DEFAULT_LOGFILE_NAME "log.txt"
#define CUSTOMPLOT_YAXIS_RANGE 120
#define VERSION_NUMBER "1.3"

MainWindow::MainWindow(ThermaldInterface *thermaldInterface) : QMainWindow(),
    temp_samples(SAMPLE_STORE_SIZE),
    currentTempsensorIndex(0),
    temp_poll_interval(DEFAULT_TEMP_POLL_INTERVAL_MS),
    m_thermaldInterface(thermaldInterface),
    logging_enabled(false),
    log_visible_only(false),
    m_plotWidget(new QCustomPlot)
{

    // Build up a color vector for holding a good variety
    colors.append(Qt::red);
    colors.append(Qt::green);
    colors.append(Qt::blue);
    colors.append(Qt::cyan);
    colors.append(Qt::magenta);
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
    colors.append(Qt::yellow);

    loadSettings();

    for (int i = 0; i < SAMPLE_STORE_SIZE; ++i) {
        temp_samples[i] = i;
    }

    setupMenus();
    setCentralWidget(m_plotWidget);
    setupPlotWidget();
    statusBar()->show();
}

void MainWindow::setupMenus()
{
    QMenu *settingsMenu = menuBar()->addMenu("&Settings");
    settingsMenu->addAction("Configure &logging", this, SLOT(configureLogging()));
    settingsMenu->addAction("Configure &trips", this, SLOT(configureTrips()));
    settingsMenu->addAction("Configure &polling interval", this, SLOT(configurePollingInterval()));
    settingsMenu->addAction("Configure &sensors", this, SLOT(configureSensors()));
    settingsMenu->addAction("&Clear settings and quit", this, SLOT(clearAndExit()));
    settingsMenu->addAction("&Quit", this, SLOT(close()));
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, SLOT(showAboutDialog()));
}

MainWindow::~MainWindow()
{
    if (logging_enabled){
        logging_file.close();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    storeSettings();
    event->accept();
}


void MainWindow::setupPlotWidget()
{
    QPen pen;
    int temp;

    // give the axes some labels:
    m_plotWidget->xAxis->setLabel("Samples");
    m_plotWidget->yAxis->setLabel("Temperature (°C)");
    // set axes ranges, so we see all data:
    m_plotWidget->xAxis->setRange(0, SAMPLE_STORE_SIZE);
    m_plotWidget->yAxis->setRange(0, CUSTOMPLOT_YAXIS_RANGE);
    m_plotWidget->legend->setVisible(true);

    pen.setWidth(5);

    currentTempsensorIndex = 0;
    int active_zone = 0;
    for (uint zone = 0; zone < m_thermaldInterface->getZoneCount(); zone++) {
        // Trips is indexed by zone. We need to make sure there is a tips item
        // (even if empty) for each zone.
        trips.append(QVector<QCPItemLine *>());

        zoneInformationType *zone_info = m_thermaldInterface->getZone(zone);
        if (!zone_info)
            continue;

        pen.setColor(colors[active_zone % colors.count()]);
        if ( zone_info->active)
            active_zone++;

        int sensor_cnt_per_zone = m_thermaldInterface->getSensorCountForZone(zone);
        if (sensor_cnt_per_zone <= 0)
            break;

        pen.setStyle(Qt::SolidLine);

        for (int cnt = 0; zone_info->active && cnt < sensor_cnt_per_zone; cnt++) {
            QString sensor_type;
            QString sensor_name;

            sensorZoneInformationType sensor_info;

            if (m_thermaldInterface->getSensorTypeForZone(zone, cnt, sensor_type) < 0)
                continue;

            sensor_name.append(zone_info->name);
            sensor_name.append(":");
            sensor_name.append(sensor_type);

            m_plotWidget->addGraph();
            m_plotWidget->graph(currentTempsensorIndex)->setName(sensor_name);
            m_plotWidget->graph(currentTempsensorIndex)->setPen(pen);
            current_sample_index[currentTempsensorIndex] = 0;
            sensor_info.index = m_thermaldInterface->getSensorIndex(sensor_type);
            sensor_info.display_name = sensor_name;
            sensor_info.sensor_name = sensor_type;
            sensor_info.zone = zone;
            sensor_types.append(sensor_info);
            sensor_visibility.append(true);

            currentTempsensorIndex++;

        }

        pen.setStyle(Qt::DashLine);

        // Draw a dashed horz line for each min valid trip temperature
        QVector<QCPItemLine *> these_trips;
        int trip_count = m_thermaldInterface->getTripCountForZone(zone);
        if (trip_count > 0) {
            for (int trip = 0; trip < trip_count; trip++){
                QCPItemLine *line = new QCPItemLine(m_plotWidget);
                temp = m_thermaldInterface->getTripTempForZone(zone, trip);
                line->start->setCoords(0, temp);
                line->end->setCoords(SAMPLE_STORE_SIZE - 1, temp);
                line->setPen(pen);
                if (zone_info->active && temp == m_thermaldInterface->getLowestValidTripTempForZone(zone)) {
                    line->setVisible(true);
                    m_thermaldInterface->setTripVisibility(zone, trip, true);
                } else {
                    line->setVisible(false);
                    m_thermaldInterface->setTripVisibility(zone, trip, false);
                }
                these_trips.append(line);
            }
            trips.last().swap(these_trips);
        }
    }

#ifdef DISPLAY_UNKNOWN
    // Now display sensors which are not part of any zone. Users can use this and assign to some zone
    for (uint i = 0; i < m_thermaldInterface->getSensorCount(); ++i) {
        sensorInformationType info;
        QString name;
        bool found = false;

        name = m_thermaldInterface->getSensorName(i);
        if (!name.isEmpty()){
            // search if this is already registered as part of a zone sensor
            for (int j = 0; j < sensor_types.count(); ++j) {
                sensorZoneInformationType sensor_info = sensor_types[j];

                if (name == sensor_info.sensor_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                sensorZoneInformationType sensor_info;
                QString sensor_name;

                sensor_name.append("UKWN:");
                sensor_name.append(name);

                m_plotWidget->addGraph();
                m_plotWidget->graph(currentTempsensorIndex)->setName(sensor_name);
                m_plotWidget->graph(currentTempsensorIndex)->setPen(pen);
                current_sample_index[currentTempsensorIndex] = 0;
                sensor_info.index = m_thermaldInterface->getSensorIndex(name);
                sensor_info.display_name = sensor_name;
                sensor_info.sensor_name = name;
                sensor_info.zone = -1;
                sensor_types.append(sensor_info);
                sensor_visibility.append(true);
                currentTempsensorIndex++;
            }

        }
    }
#endif
    connect(&tempUpdateTimer, SIGNAL(timeout()),
            this, SLOT(updateTemperatureDataSlot()));
    tempUpdateTimer.start(temp_poll_interval);
}

void MainWindow::updateTemperatureDataSlot()
{

    for (int i = 0; i < sensor_types.count(); ++i) {
        int temperature = m_thermaldInterface->getSensorTemperature(sensor_types[i].index);
            addNewTemperatureTemperatureSample(i, (double)temperature/1000.0);
        }

    if(logging_enabled) {
            outStreamLogging << Qt::endl;
    }

    m_plotWidget->replot();

    // Show any active cooling devices on the status bar
    QString str;
    for (uint i = 0; i < m_thermaldInterface->getCoolingDeviceCount(); i++) {
        int current = m_thermaldInterface->getCoolingDeviceCurrentState(i);
        int min = m_thermaldInterface->getCoolingDeviceMinState(i);
        if (current > min){
            if (str.isEmpty()){
                str += "Cooling: ";
            } else {
                str += ", ";
            }
            str += QString("%2%3 (%4/%5)")
                    .arg(m_thermaldInterface->getCoolingDeviceName(i))
                    .arg(i)
                    .arg(current)
                    .arg(m_thermaldInterface->getCoolingDeviceMaxState(i));
        }
    }
    statusBar()->showMessage(str);
}

int MainWindow::addNewTemperatureTemperatureSample(int index, double temperature)
{
    if (temperature_samples[index].size() >= SAMPLE_STORE_SIZE) {
        int i;

        for (i = 0; i < temperature_samples[index].count() - 1; i++){
            temperature_samples[index][i] = temperature_samples[index][i + 1];
        }
        temperature_samples[index][i] = temperature;
    } else {
        temperature_samples[index].push_back(temperature);
        current_sample_index[index]++;
    }
    m_plotWidget->graph(index)->setData(temp_samples, temperature_samples[index]);

    if (logging_enabled) {
        if (m_plotWidget->graph(index)->visible() || !log_visible_only){
            outStreamLogging << temperature << ", ";
        }
    }
    return 0;
}

void MainWindow::loadSettings()
{
    QSettings settings;

    // restore the previous window geometry, if available
    if (settings.contains("mainWindowGeometry/size")) {
        resize(settings.value("mainWindowGeometry/size").toSize());
        move(settings.value("mainWindowGeometry/pos").toPoint());
    } else {
        // otherwise start with the default geometry calculated from the desktop size
	QScreen *qscreen = QGuiApplication::primaryScreen();
        QRect screen = qscreen->availableGeometry();
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
    if (settings.contains("mainWindowState")) {
        restoreState(settings.value("mainWindowState").toByteArray());
    }

    // restore the polling interval, if available
    if (settings.contains("updateInterval")) {
        temp_poll_interval = settings.value("updateInterval").toUInt();
    } else {
        // otherwise load the default
        temp_poll_interval = DEFAULT_TEMP_POLL_INTERVAL_MS;
    }

    // restore the log file name, if available
    if (settings.contains("logFilename") &&
            !settings.value("logFilename").toString().isEmpty()) {
        log_filename = settings.value("logFilename").toString();
    } else {
        // otherwise load the default
        log_filename = QString(DEFAULT_LOGFILE_NAME);
    }

    // restore the log visible only setting, if available
    if (settings.contains("logVisibleOnly")) {
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
    if (!log_filename.isEmpty()) {
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

void MainWindow::changeGraphVisibilitySlot(uint index, bool visible) {
    if (index < (uint) sensor_types.count()) {
        m_plotWidget->graph(index)->setVisible(visible);
        sensor_visibility[index] = visible;

        if (index < (uint)sensor_types.count()) {
            sensorZoneInformationType sensor_info = sensor_types[index];
            if (sensor_info.zone >= 0)
                setTripVisibility(sensor_info.zone, 0, visible);
        }
    }
}

void MainWindow::changeLogVariables(bool log_enabled, bool log_vis_only,
                                    QString log_file_name)
{
    if (log_enabled && log_file_name.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setText("Please enter a valid filename.\r\rLogging not enabled");
        msgBox.exec();
        return;
    }

    /* if logging has just been turned on, open the file to write,
     * then output the sensor name header
     */
    if (!logging_enabled && log_enabled) {
        // first set the file and output stream
        logging_file.setFileName(log_file_name);
        QTextStream out(&logging_file);
        if (!logging_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "Cannot open file for writing: "
                        << qPrintable(logging_file.errorString()) << Qt::endl;
            return;
        }
        outStreamLogging.setDevice(&logging_file);

        // now output the temperature sensor names as a header
        for(int i = 0; i < sensor_types.count(); i++) {
            if (!log_vis_only || m_plotWidget->graph(i)->visible()) {
                outStreamLogging << m_plotWidget->graph(i)->name() << ", ";
            }
        }
        outStreamLogging << Qt::endl;
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

void MainWindow::setTripSetpoint(uint zone, uint trip, int temperature)
{
    trips[zone][trip]->start->setCoords(0, temperature);
    trips[zone][trip]->end->setCoords(SAMPLE_STORE_SIZE - 1, temperature);
    m_thermaldInterface->setTripTempForZone(zone, trip, temperature);
}

void MainWindow::setTripVisibility(uint zone, uint trip, bool visibility)
{
    m_thermaldInterface->setTripVisibility(zone, trip, visibility);
    trips[zone][trip]->setVisible(visibility);
}

void MainWindow::clearAndExit()
{
    QSettings settings;
    settings.clear();
    QCoreApplication::quit();
}

void MainWindow::configurePollingInterval()
{
    PollingDialog *p = new PollingDialog(this, temp_poll_interval);

    QObject::connect(p, SIGNAL(setPollInterval(uint)),
                     this, SLOT(changePollIntervalSlot(uint)));
    p->show(); // non-modal
}

void MainWindow::configureSensors()
{
    SensorsDialog dialog(this);

    // set the checkbox names to match the temperature sensor names
    for (int i = 0; i < sensor_visibility.count(); i++) {
        dialog.addSensor(sensor_types[i].display_name, sensor_visibility[i]);
    }

    if (dialog.exec() == QDialog::Rejected) {
        return;
    }

    sensor_visibility = dialog.getVisibilities();
    for (int i=0; i<sensor_visibility.count(); i++) {
        changeGraphVisibilitySlot(i, sensor_visibility[i]);
    }
}

void MainWindow::configureLogging()
{
    LogDialog *l = new LogDialog(this);

    QObject::connect(l, SIGNAL(setLogVariable(bool, bool, QString)),
                     this, SLOT(changeLogVariables(bool, bool, QString)));
    l->setLoggingState(logging_enabled, log_visible_only, log_filename);
    l->show();
}

void MainWindow::showAboutDialog()
{
    QString str;
    str = QString("<h3>Thermal Monitor %1</h3>"
                  "<p>GUI for Linux thermal daemon (thermald)</p>"
                  "<p>Copyright (c) 2022, Intel Corporation</p>"
                  "<p>This program comes with ABSOLUTELY NO WARRANTY</p>"
                  "<p>This work is licensed under GPL v3</p>"
                  "<p>Refer to https://www.gnu.org/licenses/gpl-3.0.txt</p>")
            .arg(QString(VERSION_NUMBER));
    QMessageBox::about(this, "About Thermal Monitor", str);
}

void MainWindow::configureTrips()
{
    tripsDialog *t = new tripsDialog(this, m_thermaldInterface);

    QObject::connect(t, SIGNAL(setTripVis(uint, uint, bool)),
                     this, SLOT(setTripVisibility(uint, uint, bool)));
    QObject::connect(t, SIGNAL(changeTripSetpoint(uint, uint, int)),
                     this, SLOT(setTripSetpoint(uint, uint, int)));
    for(uint i = 0; i < m_thermaldInterface->getZoneCount(); i++) {
        t->addZone(m_thermaldInterface->getZone(i));
    }

    t->show();
}
