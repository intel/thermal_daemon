# -*- coding: utf-8 -*-

# script to dump thermal config from the running thermald via dbus
# The caller must be in "power" group

import dbus
import os

def dump_thermal_config():

    if os.path.exists(file_name):
        os.remove(file_name)

    f_handle = open(file_name, 'a')

    system_bus = dbus.SystemBus()

    thd = system_bus.get_object('org.freedesktop.thermald',
                                '/org/freedesktop/thermald')
    thd_intf = dbus.Interface(thd,
                              dbus_interface='org.freedesktop.thermald')

    zone_count = thd_intf.GetZoneCount()
    string_buffer = 'zones count:' + str(zone_count) + '\n'
    f_handle.write(string_buffer)

    sensor_count = thd_intf.GetSensorCount()
    string_buffer = 'sensors count:' + str(sensor_count) + '\n'
    f_handle.write(string_buffer)
    f_handle.write('\n')

    for i in range(zone_count):
        zone = thd_intf.GetZoneInformation(i)

        string_buffer = 'zone id' + ': ' + str(i) + '''

'''
        f_handle.write(string_buffer)

        string_buffer = '\tzone name' + ': ' + zone[0] + '\n'
        f_handle.write(string_buffer)

        string_buffer = '\tSensor Count' + ': ' + str(zone[1]) + '\n'
        f_handle.write(string_buffer)

        for j in range(zone[1]):
            sensor = thd_intf.GetZoneSensorAtIndex(i, j)
            string_buffer = '\t\t sensor: ' + sensor + '\n'
            f_handle.write(string_buffer)

        string_buffer = '\tTrip Count' + ': ' + str(zone[2]) + '\n'
        f_handle.write(string_buffer)

        for j in range(zone[2]):
            string_buffer = '\tTrip id' + ': ' + str(j) + '\n'
            f_handle.write(string_buffer)

            trip = thd_intf.GetZoneTripAtIndex(i, j)

            string_buffer = '\t\t\t trip temp' + ': ' + str(trip[0]) \
                + '\n'
            f_handle.write(string_buffer)

            string_buffer = '\t\t\t trip type' + ': ' + str(trip[1]) \
                + '\n'
            f_handle.write(string_buffer)

            string_buffer = '\t\t\t sensor id' + ': ' + str(trip[2]) \
                + '\n'
            f_handle.write(string_buffer)

        string_buffer = '''

'''
        f_handle.write(string_buffer)

    cdev_count = thd_intf.GetCdevCount()
    string_buffer = 'cdev count:' + str(cdev_count) + '\n'
    f_handle.write(string_buffer)
    f_handle.write('\n')

    for i in range(cdev_count):
        string_buffer = 'cdev id' + ': ' + str(i) + '\n'
        f_handle.write(string_buffer)

        cdev = thd_intf.GetCdevInformation(i)
        string_buffer = '\t\t type' + ': ' + str(cdev[0]) + '\n'
        f_handle.write(string_buffer)

        string_buffer = '\t\t min_state' + ': ' + str(cdev[1]) + '\n'
        f_handle.write(string_buffer)

        string_buffer = '\t\t max_state' + ': ' + str(cdev[2]) + '\n'
        f_handle.write(string_buffer)

        string_buffer = '\t\t current_state' + ': ' + str(cdev[3]) \
            + '\n'
        f_handle.write(string_buffer)

    f_handle.close()


def remove_space(string):
    return string.replace(' ', '')


if __name__ == '__main__':
    product_name = '/sys/class/dmi/id/product_name'

    try:
        file_name = open(product_name, 'r').read()
    except:
        print ('Error opening ', file_name)
        sys.exit(2)

    file_name = '/tmp/' + file_name.strip()
    file_name = remove_space(file_name) + '.config'

    print ('File Name to dump config:', file_name)

    dump_thermal_config()

