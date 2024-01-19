#!/bin/bash

echo "****thermald preference****"
echo "0 : DEFAULT"
echo "1 : PERFORMANCE"
echo "2 : ENERGY_CONSERVE"
echo "3 : DISABLED"
echo "4 : CALIBRATE"
echo "5 : SET USER DEFINED CPU MAX temp"
echo "6 : SET USER DEFINED CPU PASSIVE temp"
echo "7 : TERMINATE"
echo "8 : REINIT"
echo "A : Add sensor test"
echo "B : Get Sensor Information"
echo "C : Add zone test"
echo "D : Set zone test"
echo "E : Get zone test"
echo "F : Delete zone test"
echo "G : Add cdev test"
echo "H : Get Sensor Count"
echo "I : Get Zone Count"
echo "J : Get Zone Information"
echo "K : Get Zone Sensor Information"
echo "L : Get Zone Trip Information"
echo "M : Get cdev count"
echo "N : Get cdev Information"

arg="0"
if [[ "$1" != "" ]]; then
        if [[ "$2" != "" ]]; then
		arg=$2
	fi

	opt_no=$1
	echo "Taking command line argument as choice!"
else
	echo -n " Enter thermald preference [1..6]: "
	read opt_no
fi

case $opt_no in
0) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetCurrentPreference string:"FALLBACK"
;;

1) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetCurrentPreference string:"PERFORMANCE"
;;

2) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetCurrentPreference string:"ENERGY_CONSERVE"
;;

3) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetCurrentPreference string:"DISABLE"
;;

4) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Calibrate
;;

5)
echo -n " Enter valid max temp in mill degree celsius "
read max_temp
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetUserMaxTemperature string:cpu uint32:$max_temp
;;

6)
echo -n " Enter valid passive temp in mill degree celsius "
read psv_temp
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetUserPassiveTemperature string:cpu uint32:$psv_temp
;;

7) dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.Terminate
;;

8) dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.Reinit
;;

A)
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.AddSensor string:"TEST_ADD_SENSOR" string:"/sys/class/thermal/thermal_zone0/temp"
;;

B)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetSensorInformation uint32:0
;;

C)
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.AddZonePassive string:"TEST_ADD_ZONE" uint32:90000 string:"hwmon" string:"intel_pstate"
;;

D)
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetZoneStatus string:"TEST_ADD_ZONE" int32:0
;;

E)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetZoneStatus string:"TEST_ADD_ZONE"
;;

F)
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.DeleteZone string:"TEST_ADD_ZONE"
;;

G)
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.AddCoolingDevice string:"TEST_CDEV" string:"/sys/class/thermal/cooling_device0/cur_state" int32:0 int32:1 int32:1
;;

H)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetSensorCount
;;

I)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetZoneCount
;;

J)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetZoneInformation uint32:0
;;

K)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetZoneSensorAtIndex uint32:0 uint32:$arg
;;

L)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetZoneTripAtIndex uint32:0 uint32:$arg
;;

M)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetCdevCount
;;

N)
dbus-send --system --dest=org.freedesktop.thermald --print-reply /org/freedesktop/thermald org.freedesktop.thermald.GetCdevInformation uint32:$arg
;;

*) echo "Invalid option"

esac
