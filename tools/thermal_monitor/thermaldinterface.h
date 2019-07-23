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

#ifndef THERMALDINTERFACE_H
#define THERMALDINTERFACE_H

#include <QtCore/QCoreApplication>
#include <QtDBus/QtDBus>

#define SERVICE_NAME            "org.freedesktop.thermald"
#define MAX_TEMP_INPUT_COUNT 64

enum {
	CRITICAL_TRIP,
	HOT_TRIP,
	MAX_TRIP,
	PASSIVE_TRIP,
	ACTIVE_TRIP,
	POLLING_TRIP,
	INVALID_TRIP
};

typedef struct {
    QString name;
    int min_state;
    int max_state;
    int current_state;
} coolingDeviceInformationType;

typedef struct {
    QString name;
    QString path;
    int temperature;
} sensorInformationType;

typedef struct {
    int temp;
    int trip_type;
    bool visible;
    int sensor_id;
    int cdev_size;
    QList<int> cdev_ids; // Not currently using
} tripInformationType;

typedef struct {
    uint id;
    QString name;
    uint sensor_count;
    uint trip_count;
    QVector<tripInformationType> trips;
    uint lowest_valid_index;
    int active;
} zoneInformationType;

class ThermaldInterface {
public:
    ThermaldInterface();
    ~ThermaldInterface();

    bool initialize();

    uint getCoolingDeviceCount();
    QString getCoolingDeviceName(uint index);
    int getCoolingDeviceMinState(uint index);
    int getCoolingDeviceMaxState(uint index);
    int getCoolingDeviceCurrentState(uint index);

    uint getSensorCount();
    QString getSensorName(uint index);
    QString getSensorPath(uint index);
    int getSensorTemperature(uint index);
    int getSensorIndex(QString sensor_type);

    int getSensorCountForZone(uint zone);
    int getSensorTypeForZone(uint zone, uint index, QString &sensor_type);

    int getLowestValidTripTempForZone(uint zone);
    int getTripCountForZone(uint zone);
    int getTripTempForZone(uint zone, uint trip);
    int getTripTypeForZone(uint zone, uint trip);
    zoneInformationType* getZone(uint zone);
    uint getZoneCount();
    QString getZoneName(uint zone);
    int setTripTempForZone(uint zone, uint trip, int temperature);

    bool tripVisibility(uint zone, uint trip);
    void setTripVisibility(uint zone, uint trip, bool visibility);

private:
    QDBusInterface *iface;
    QVector<coolingDeviceInformationType> cooling_devices;
    QVector<sensorInformationType> sensors;
    QVector<zoneInformationType> zones;

    int getCoolingDeviceInformation(uint index, coolingDeviceInformationType &info);
    int getSensorInformation(uint index, sensorInformationType &info);
    int getTripInformation(uint zone_index, uint trip_index, tripInformationType &info);
    int getZoneInformation(uint index, zoneInformationType &info);
};


#endif

