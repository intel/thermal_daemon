#!/bin/sh

echo "****thermald preference****"
echo "0 : DEFAULT"
echo "1 : PERFORMANCE"
echo "2 : ENERGY_CONSERVE"
echo "3 : DISABLED"
echo "4 : CALIBRATE"
echo "5 : TERMINATE"
echo -n " Enter thermald preference [1..3]: "
read opt_no

case $opt_no in
0) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"FALLBACK"
;;

1) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"PERFORMANCE"
;;

2) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"ENERGY_CONSERVE"
;;

3) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"DISABLE"
;;

4) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.Calibrate string:"CALIBRATE"
;;

5) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.Terminate string:"TERMINATE"
;;

*) echo "Invalid option"

esac
