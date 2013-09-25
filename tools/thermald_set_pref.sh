#!/bin/sh

echo "****thermald preference****"
echo "0 : DEFAULT"
echo "1 : PERFORMANCE"
echo "2 : ENERGY_CONSERVE"
echo "3 : DISABLED"
echo "4 : CALIBRATE"
echo "5 : SET USER DEFINED SET POINT"
echo "6 : TERMINATE"
echo -n " Enter thermald preference [1..3]: "
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

4) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Calibrate string:"CALIBRATE"
;;

5)
echo -n " Enter valid max temp in mill degree celsius "
read max_temp
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.SetUserSetPoint string:$max_temp
;;

6) dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Terminate string:"TERMINATE"
;;

*) echo "Invalid option"

esac
