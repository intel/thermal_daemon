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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QLabel>
#include <string>
#include <qcustomplot.h>
#include "thermaldinterface.h"

typedef struct {
    QString display_name;
    QString sensor_name;
    int index;
    int zone;
} sensorZoneInformationType;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ThermaldInterface * thermaldInterface);
    ~MainWindow();

    void setupPlotWidget();
    int addNewTemperatureTemperatureSample(int index, double temperature);
    bool getVisibleState(int index);

protected:
    virtual void closeEvent(QCloseEvent *event);

public slots:
    void changePollIntervalSlot(uint new_val);
    void changeGraphVisibilitySlot(uint index, bool visible);
    void changeLogVariables(bool log_enabled, bool log_vis_only,
                        QString log_file_name);
    void setTripSetpoint(uint zone, uint trip, int temperature);
    void setTripVisibility(uint zone, uint trip, bool visibility);

    void updateTemperatureDataSlot();

private slots:
    void clearAndExit();
    void configurePollingInterval();
    void configureSensors();
    void configureLogging();
    void showAboutDialog();
    void configureTrips();

private:
    void setupMenus();

    QTimer tempUpdateTimer;
    QVector<QColor> colors;
    QVector<double> temp_samples;
    int currentTempsensorIndex;
    QVector<double> temperature_samples[MAX_TEMP_INPUT_COUNT];
    int current_sample_index[MAX_TEMP_INPUT_COUNT];
    uint temp_poll_interval;
    QVector<bool> sensor_visibility;
   // QLabel *sensor_label;
    QVector<QVector<QCPItemLine *> > trips;
    QVector<sensorZoneInformationType>sensor_types;

    ThermaldInterface *m_thermaldInterface;

    bool logging_enabled;
    bool log_visible_only;
    QString log_filename;
    QFile logging_file;
    QTextStream outStreamLogging;
    QCustomPlot *m_plotWidget;

    void loadSettings();
    void storeSettings();
};

#endif // MAINWINDOW_H
