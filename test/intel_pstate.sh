#!/bin/bash

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

echo "Executing test : Test intel_pstate cooling"
cp intel_pstate.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:x86_pkg_temp | sed 's/\/type.*//')
min_perf_pct=50
max_perf_pct=100
echo "min_perf_pct:" $min_perf_pct
echo "max_perf_pct:" $max_perf_pct

cat ${THD0_ZONE}/temp
sleep 2
echo "Forcing to throttle"
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/intel_pstate/max_perf_pct)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -le $min_perf_pct ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -gt $min_perf_pct ]; then
	echo "intel_pstate: Step 0: Test failed"
	exit 1
else
	echo "intel_pstate: Step 0: Test passed"
fi

echo "Removing throttle slowly stepwise"
echo 69000 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/intel_pstate/max_perf_pct)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $max_perf_pct ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $max_perf_pct ]; then
	echo "intel_state: Step 1: Test failed"
	exit 1
else
	echo "intel_psatte: Step 1: Test passed"
fi

echo "Forcing throttle again "
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/intel_pstate/max_perf_pct)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -le $min_perf_pct ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -gt $min_perf_pct ]; then
	echo "intel_pstate: Step 0: Test failed"
	exit 1
else
	echo "intel_pstate: Step 0: Test passed"
fi

echo "Removing throttle in one step"
echo 0 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	scaling_max_freq=$(cat /sys/devices/system/cpu/intel_pstate/max_perf_pct)
	echo "current state " ${scaling_max_freq}
	if [ $scaling_max_freq -eq $max_perf_pct ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $scaling_max_freq -ne $max_perf_pct ]; then
	echo "intel_pstate: Step 2: Test failed"
	exit 1
else
	echo "intel_pstate: Step 2: Test passed"
fi

