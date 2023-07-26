#!/bin/bash

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

echo "Executing test : Test cpufreq cooling"
cp cpufreq.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:x86_pkg_temp | sed 's/\/type.*//')
cpuinfo_min_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq)
cpuinfo_max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq)
echo "cpuinfo_min_freq:" $cpuinfo_min_freq
echo "cpuinfo_max_freq:" $cpuinfo_max_freq

cat ${THD0_ZONE}/temp
sleep 2
echo "Forcing to throttle"
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $cpuinfo_min_freq ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $cpuinfo_min_freq ]; then
	echo "cpufreq: Step 0: Test failed"
	exit 1
else
	echo "cpufreq: Step 0: Test passed"
fi

echo "Removing throttle slowly stepwise"
echo 69000 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $cpuinfo_max_freq ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $cpuinfo_max_freq ]; then
	echo "cpufreq: Step 1: Test failed"
	exit 1
else
	echo "cpufreq: Step 1: Test passed"
fi

echo "Forcing throttle again "
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $cpuinfo_min_freq ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $cpuinfo_min_freq ]; then
	echo "cpufreq: Step 0: Test failed"
	exit 1
else
	echo "cpufreq: Step 0: Test passed"
fi

echo "Removing throttle in one step"
echo 0 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $cpuinfo_max_freq ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $cpuinfo_max_freq ]; then
	echo "cpufreq: Step 2: Test failed"
	exit 1
else
	echo "cpufreq: Step 2: Test passed"
fi

