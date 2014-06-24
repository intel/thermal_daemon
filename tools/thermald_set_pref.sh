#!/bin/sh

echo "****thermald preference****"
echo "0 : DEFAULT"
echo "1 : PERFORMANCE"
echo "2 : ENERGY_CONSERVE"
echo "3 : DISABLED"
echo "4 : CALIBRATE"
echo "5 : SET USER DEFINED CPU MAX temp"
echo "6 : SET USER DEFINED CPU PASSIVE temp"
echo "7 : TERMINATE"
echo -n " Enter thermald preference [1..6]: "
read opt_no

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
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetUserMaxTemperature string:cpu string:$max_temp
;;

6)
echo -n " Enter valid max temp in mill degree celsius "
read psv_temp
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetUserPassiveTemperature string:cpu string:$psv_temp
;;

7) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Terminate
;;

*) echo "Invalid option"

esac
