#!/usr/bin/python
# -*- coding: utf-8 -*-

# script to dump temperature samples every 10 seconds
# The caller must be in "power" group to call the script

import dbus
import os
import time


def dump_temperature():

    if os.path.exists(file_name):
        os.remove(file_name)

    f_handle = open(file_name, 'a')

    system_bus = dbus.SystemBus()

    thd = system_bus.get_object('org.freedesktop.thermald',
                                '/org/freedesktop/thermald')
    thd_intf = dbus.Interface(thd,
                              dbus_interface='org.freedesktop.thermald')

    sensor_count = thd_intf.GetSensorCount()
    string_buffer = 'sensor count:' + str(sensor_count) + '\n'
    f_handle.write(string_buffer)
    f_handle.write('\n')

    string_buffer = ''
    for i in range(sensor_count):
        sensor_info = thd_intf.GetSensorInformation(i)
        string_buffer = string_buffer + sensor_info[0] + ','

    f_handle.write(string_buffer)
    f_handle.write('\n')

    string_buffer = ''

    f_lock = open('/tmp/thermal_temp_dump.lock', 'a')
    f_lock.close()

    while True:
        if os.path.exists('/tmp/thermal_temp_dump.lock') == False:
            break
        string_buffer = ''
        for i in range(sensor_count):
            sensor = thd_intf.GetSensorTemperature(i)
            string_buffer = string_buffer + str(sensor / 1000) + ','
        f_handle.write(string_buffer)
        f_handle.write('\n')
        time.sleep(10)

    f_handle.close()


def remove_spaces(string):
    return string.replace(' ', '')


if __name__ == '__main__':
    product_name = '/sys/class/dmi/id/product_name'

    try:
        file_name = open(product_name, 'r').read()
    except:
        print ('Error opening ', file_name)
        sys.exit(2)

    file_name = '/tmp/' + file_name.strip()
    file_name = remove_spaces(file_name) + '.csv'

    print ('File Name to dump config:', file_name)
    print ('Before starting this script, do the following steps')
    print ('sudo touch /var/run/thermald/debug_mode')
    print ('sudo systemctl restart thermald')
    print ('This will also dump power data from rapl as samples')
    print ('This script will loop forever till user deletes file')
    print ('/tmp/thermal_temp_dump.lock')

    dump_temperature()
