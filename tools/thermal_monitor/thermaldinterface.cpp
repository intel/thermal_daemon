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

#include <QtDebug>
#include <QTreeWidgetItem>
#include "thermaldinterface.h"
#include "tripsdialog.h"

ThermaldInterface::ThermaldInterface() :
    iface(NULL)
{

}

ThermaldInterface::~ThermaldInterface()
{
    delete iface;
}

bool ThermaldInterface::initialize()
{
    /* Create the D-Bus interface
     * Get the temperature sensor count and information from each sensor
     * Get the cooling device count and information from each device
     */
    if (!QDBusConnection::systemBus().isConnected()) {
        qCritical()  << "Cannot connect to the D-Bus session bus";
        return false;
    }

    iface = new QDBusInterface(SERVICE_NAME, "/org/freedesktop/thermald",
                               "org.freedesktop.thermald", QDBusConnection::systemBus());
    if (!iface->isValid()) {
        qCritical()  << "Cannot connect to interface" << SERVICE_NAME;
        return false;
    }

    // get temperature sensor count
    uint sensor_count;
    QDBusReply<uint> count  = iface->call("GetSensorCount");
    if (count.isValid()) {
        if (count <= MAX_TEMP_INPUT_COUNT){
            sensor_count = count;
        } else {
            qCritical() << "error: input sensor count of" << count
                        << "is larger than system maximum (" << MAX_TEMP_INPUT_COUNT
                        << ")";
            return false;
        }
    } else {
        qCritical() << "error from" << iface->interface() << "=" << count.error();
        return false;
    }

    // Read in all the the temperature sensor data from the thermal daemon
    for (uint i = 0; i < sensor_count; i++){
        sensorInformationType new_sensor;

        if (getSensorInformation(i, new_sensor) >= 0){
            sensors.append(new_sensor);
        }
    }
    if (sensor_count != (uint)sensors.count()){
        qCritical() << "error: ThermaldInterface::getSensorCount() sensor_count != sensors.count()),"
                    << sensor_count << "vs" << sensors.count();
    }

    // get cooling device count
    uint cooling_device_count;
    QDBusReply<uint> cdev_count  = iface->call("GetCdevCount");
    if (cdev_count.isValid()) {
        cooling_device_count = cdev_count;
    } else {
        qCritical() << "error from" << iface->interface() << "=" << cdev_count.error();
        return false;
    }

    // Read in all the the cooling device data from the thermal daemon
    for (uint i = 0; i < cooling_device_count; i++){
        coolingDeviceInformationType new_device;

        if (getCoolingDeviceInformation(i, new_device) >= 0){
            cooling_devices.append(new_device);
        }
    }
    if (cooling_device_count != (uint)cooling_devices.count()){
        qCritical() << "error: ThermaldInterface::getCoolingDeviceCount()"
                    << " cooling_device_count != cooling_devices.count(),"
                    << cooling_device_count << "vs" << cooling_devices.count();
    }

    // get zone count
    uint zone_count;
    QDBusReply<uint> z_count  = iface->call("GetZoneCount");
    if (z_count.isValid()) {
        zone_count = z_count;
    } else {
        qCritical() << "error from" << iface->interface() << "=" << z_count.error();
        return false;
    }

    // Read in all the the zone data from the thermal daemon
    for (uint i = 0; i < zone_count; i++){
        zoneInformationType new_zone;

        if (getZoneInformation(i, new_zone) >= 0){
            zones.append(new_zone);
        }
    }
    if (zone_count != (uint)zones.count()){
        qCritical() << "error: ThermaldInterface::getCoolingDeviceCount()"
                    << " zone_count != zones.count(),"
                    << zone_count << "vs" << zones.count();
    }

    // Read in all the the trip data from the thermal daemon
    for (uint i = 0; i < zone_count; i++){
        uint lowest_valid_index = (uint)zones[i].trip_count;
        for (uint j = 0; j < (uint)zones[i].trip_count; j++){
            tripInformationType new_trip;

            if (getTripInformation(i, j, new_trip) >= 0){
                zones[i].trips.append(new_trip);
                // Figure out the lowest valid trip index
                if (new_trip.trip_type == CRITICAL_TRIP ||
                        new_trip.trip_type == PASSIVE_TRIP){
                    if (j < lowest_valid_index){
                        lowest_valid_index = j;
                    }
                }
            }
        }
        // Store the actual number of trips, as opposed to the theoretical maximum
        zones[i].trip_count = zones[i].trips.count();

        // Store the first valid trip temp for the zone
        if (lowest_valid_index < (uint)zones[i].trip_count){
            zones[i].lowest_valid_index = lowest_valid_index;
            //trip_temps.append(zones[i].trips[lowest_valid_index].temp);
        } else {
            qCritical() << "error: ThermaldInterface::initialize()"
                        << " could not find valid trip for zone"
                        << i;
        }
        /* Yo!  zones[i].trip_count is the theoretical count
         * but  zones[i].trips.count() is the actual valid count
         *
         * So when you want the trip count for any given zone,
         * use zones[i].trips.count()
         */
    }
    return true;
}

uint ThermaldInterface::getCoolingDeviceCount(){
    return cooling_devices.count();
}

QString ThermaldInterface::getCoolingDeviceName(uint index)
{
    if (index < (uint)cooling_devices.count()){
        return cooling_devices[index].name;
    } else {
        qCritical() << "error: ThermaldInterface::getCoolingDeviceName"
                    << "index" << index << "is >= than cooling_device.count"
                    << cooling_devices.count();
        return QString("");
    }
}

int ThermaldInterface::getCoolingDeviceMinState(uint index)
{
    if (index < (uint)cooling_devices.count()){
        return cooling_devices[index].min_state;
    } else {
        qCritical() << "error: ThermaldInterface::getCoolingMinState"
                    << "index" << index << "is >= than cooling_device.count"
                    << cooling_devices.count();
        return -1;
    }
}

int ThermaldInterface::getCoolingDeviceMaxState(uint index)
{
    if (index < (uint)cooling_devices.count()){
        return cooling_devices[index].max_state;
    } else {
        qCritical() << "error: ThermaldInterface::getCoolingMaxState"
                    << "index" << index << "is >= than cooling_device.count"
                    << cooling_devices.count();
        return -1;
    }

}

int ThermaldInterface::getCoolingDeviceCurrentState(uint index)
{
    if (index < (uint)cooling_devices.count()){
        coolingDeviceInformationType cdev;

        // The state may have changed, so poll it again
        getCoolingDeviceInformation(index, cdev);
        cooling_devices[index].current_state = cdev.current_state;
        return cooling_devices[index].current_state;
    } else {
        qCritical() << "error: ThermaldInterface::getCoolingCurrentState"
                    << "index" << index << "is >= than cooling_device.count"
                    << cooling_devices.count();
        return -1;
    }
}

int ThermaldInterface::getCoolingDeviceInformation(uint index, coolingDeviceInformationType &info)
{
    QDBusMessage result;
    result = iface->call("GetCdevInformation", index);

    if (result.type() == QDBusMessage::ReplyMessage) {
        info.name = result.arguments().at(0).toString();
        info.min_state = result.arguments().at(1).toInt();
        info.max_state = result.arguments().at(2).toInt();
        info.current_state = result.arguments().at(3).toInt();
        return 0;
    } else {
        qCritical() << "error from" << iface->interface() <<  result.errorMessage();
        return -1;
    }
}

uint ThermaldInterface::getSensorCount()
{
    return sensors.count();
}


int ThermaldInterface::getSensorInformation(uint index,
                                            sensorInformationType &info)
{
    QDBusMessage result;

    result = iface->call("GetSensorInformation", index);
    if (result.type() == QDBusMessage::ReplyMessage) {
        info.name = result.arguments().at(0).toString();
        info.path = result.arguments().at(1).toString();
        info.temperature = result.arguments().at(2).toInt();
        return 0;
    } else {
        qCritical() << "error from" << iface->interface() <<  result.errorMessage();
        return -1;
    }
}

QString ThermaldInterface::getSensorName(uint index){
    if (index < (uint)sensors.size()){
        return sensors[index].name;
    } else {
        return QString("");
    }
}

QString ThermaldInterface::getSensorPath(uint index){
    if (index < (uint)sensors.size()){
        return sensors[index].path;
    } else {
        return QString("");
    }
}

int ThermaldInterface::getSensorTemperature(uint index)
{
    QDBusReply<uint> temp  = iface->call("GetSensorTemperature", index);
    if (temp.isValid()) {
        return temp;
    } else {
        qCritical() << "error from" << iface->interface() << "=" << temp.error();
        return -1;
    }
}

int ThermaldInterface::getTripInformation(uint zone_index,
                                          uint trip_index, tripInformationType &info)
{
    QDBusMessage result;

    result = iface->call("GetZoneTripAtIndex", zone_index, trip_index);

    if (result.type() == QDBusMessage::ReplyMessage) {
        info.temp = result.arguments().at(0).toInt() / 1000;
        info.trip_type = result.arguments().at(1).toInt();
        info.sensor_id = result.arguments().at(2).toInt();
        info.cdev_size = result.arguments().at(3).toInt();
        //        info.cdev_ids = result.arguments().at(4).toList();
        return 0;
    } else if (result.signature().isEmpty()){
        // If we get and empty response, then ignore it, but return error code
        return -1;
    } else {
        qCritical() << "error from" << iface->interface() <<  result.errorMessage()
                    << zone_index << trip_index;
        return -1;
    }
}

int ThermaldInterface::getLowestValidTripTempForZone(uint zone)
{
    if (zone < (uint)zones.count()){
        return zones[zone].trips[zones[zone].lowest_valid_index].temp;
    } else {
        qCritical() << "error: ThermaldInterface::getLowestValidTripTempForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

int ThermaldInterface::getSensorCountForZone(uint zone)
{
    if (zone < (uint)zones.count()){
        return (uint)zones[zone].sensor_count;
    } else {
        qCritical() << "error: ThermaldInterface::getSensorCountForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

int ThermaldInterface::getSensorTypeForZone(uint zone, uint index, QString &sensor_type)
{
    QDBusMessage result;
    result = iface->call("GetZoneSensorAtIndex", zone, index);

    if (result.type() == QDBusMessage::ReplyMessage) {
        sensor_type = result.arguments().at(0).toString();
        return 0;
    } else {
        qCritical() << "error from" << iface->interface() <<  result.errorMessage();
        return -1;
    }
}

int ThermaldInterface::getSensorIndex(QString sensor_type)
{
    for (uint i = 0; i < (uint)sensors.count(); i++){
        if(sensor_type == sensors[i].name)
                return (int)i;
    }
    return -1;
}

int ThermaldInterface::getTripCountForZone(uint zone)
{
    /* Yo!  zones[i].trip_count is the theoretical count
     * but  zones[i].trips.count() is the actual valid count
     *
     * So when you want the trip count for any given zone,
     * use zones[i].trips.count()
     */
    if (zone < (uint)zones.count()){
        return (uint)zones[zone].trips.count();
    } else {
        qCritical() << "error: ThermaldInterface::getTripCountForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

int ThermaldInterface::getTripTempForZone(uint zone, uint trip)
{

    if (zone < (uint)zones.count()){
        if (trip < zones[zone].trip_count){
            return zones[zone].trips[trip].temp;
        } else {
            qCritical() << "error: ThermaldInterface::getTripTempForZone"
                        << "trip index" << trip << "is >= than trip count"
                        << zones[zone].trip_count;
            return -1;
        }
    } else {
        qCritical() << "error: ThermaldInterface::getTripTempForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

int ThermaldInterface::getTripTypeForZone(uint zone, uint trip)
{
    if (zone < (uint)zones.count()){
        if (trip < zones[zone].trip_count){
            return zones[zone].trips[trip].trip_type;
        } else {
            qCritical() << "error: ThermaldInterface::getTripTypeForZone"
                        << "trip index" << trip << "is >= than trip count"
                        << zones[zone].trip_count;
            return -1;
        }
    } else {
        qCritical() << "error: ThermaldInterface::getTripTypeForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

zoneInformationType* ThermaldInterface::getZone(uint zone)
{
    if (zone < (uint)zones.count()){
        return &zones[zone];
    } else {
        qCritical() << "error: ThermaldInterface::getZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return NULL;
    }
}

uint ThermaldInterface::getZoneCount()
{
    return (uint)zones.count();
}

QString ThermaldInterface::getZoneName(uint zone)
{
    if (zone < (uint)zones.count()){
        return zones[zone].name;
    } else {
        qCritical() << "error: ThermaldInterface::getZoneName"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return QString("");
    }
}

int ThermaldInterface::getZoneInformation(uint index, zoneInformationType &info)
{
    QDBusMessage result;
    result = iface->call("GetZoneInformation", index);

    if (result.type() == QDBusMessage::ReplyMessage) {
        info.name = result.arguments().at(0).toString();
        info.sensor_count = result.arguments().at(1).toInt();
        info.trip_count = result.arguments().at(2).toInt();
        info.active = result.arguments().at(3).toInt();
        info.lowest_valid_index = 0;
        info.id = index;
        return 0;
    } else {
        qCritical() << "error from" << iface->interface() <<  result.errorMessage();
        return -1;
    }
}

int ThermaldInterface::setTripTempForZone(uint zone, uint trip, int temperature)
{
    if (zone < (uint)zones.count()){
        if (trip < zones[zone].trip_count){
            zones[zone].trips[trip].temp = temperature;
            // tell thermald to change the temperature for max and passive
            if (zones[zone].trips[trip].trip_type == MAX_TRIP){
                iface->call("SetUserMaxTemperature", zones[zone].name,
                            (uint)(temperature * 1000));
                return 0;
            } else if (zones[zone].trips[trip].trip_type == PASSIVE_TRIP){
                iface->call("SetUserPassiveTemperature", zones[zone].name,
                            (uint)(temperature * 1000));
                // Call reinit from thermald in order to get the change to stick
                iface->call("Reinit");
                return 0;
            } else {
                qCritical() << "error: ThermaldInterface::setTripTempForZone"
                            << "trip type is not 'max' or 'passive'"
                            << "zone" << zone << "trip" << trip;
                return -1;
            }
        } else {
            qCritical() << "error: ThermaldInterface::setTripTempForZone"
                        << "trip index" << trip << "is >= than trip count"
                        << zones[zone].trip_count;
            return -1;
        }
    } else {
        qCritical() << "error: ThermaldInterface::setTripTempForZone"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return -1;
    }
}

void ThermaldInterface::setTripVisibility(uint zone, uint trip, bool visibility)
{
    if (zone < (uint)zones.count()){
        if (trip < zones[zone].trip_count){
            zones[zone].trips[trip].visible = visibility;
        } else {
            qCritical() << "error: ThermaldInterface::setTripVisibility"
                        << "trip index" << trip << "is >= than trip count"
                        << zones[zone].trip_count;
        }
    } else {
        qCritical() << "error: ThermaldInterface::setTripVisibility"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
    }
}

bool ThermaldInterface::tripVisibility(uint zone, uint trip)
{
    if (zone < (uint)zones.count()){
        if (trip < zones[zone].trip_count){
            return zones[zone].trips[trip].visible;
        } else {
            qCritical() << "error: ThermaldInterface::tripVisibility"
                        << "trip index" << trip << "is >= than trip count"
                        << zones[zone].trip_count;
            return false;
        }
    } else {
        qCritical() << "error: ThermaldInterface::tripVisibility"
                    << "zone index" << zone << "is >= than zone count"
                    << zones.count();
        return false;
    }
}

