#!/bin/sh

echo "****thermald preference****"
echo "1 : PERFORMANCE"
echo "2 : ENERGY_CONSERVE"
echo "3 : DISABLED"
echo -n " Enter thermald preference [1..3]: "
read opt_no

case $opt_no in

1) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"PERFORMANCE"
;;

2) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"ENERGY_CONSERVE"
;;

3) dbus-send --dest=org.thermald.control /org/thermald/settings org.thermald.value.SetCurrentPreference string:"DISABLE"
;;

*) echo "Invalid option"

esac
