#!/bin/bash

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

echo "Executing test : Test intel_rapl cooling"
cp intel_rapl.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:x86_pkg_temp | sed 's/\/type.*//')
#rapl_max_power=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_max_power_uw)
#rapl_min_power=$(expr $rapl_max_power / 2 )
rapl_max_power=25000000
rapl_min_power=10000000
echo "rapl_min_power:" $rapl_min_power
echo "rapl_max_power:" $rapl_max_power

cat ${THD0_ZONE}/temp
sleep 2
echo "Forcing to throttle"
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_power_limit=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_power_limit_uw)
	echo "current state " ${curr_power_limit}
	if [ $curr_power_limit -le $rapl_min_power ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_power_limit -gt $rapl_min_power ]; then
	echo "intel_rapl: Step 0: Test failed"
	exit 1
else
	echo "intel_rapl: Step 0: Test passed"
fi

echo "Removing throttle slowly stepwise"
echo 69000 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_power_limit=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_power_limit_uw)
	echo "current state " ${curr_power_limit}
	if [ $curr_power_limit -ge $rapl_max_power ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_power_limit -lt $rapl_max_power ]; then
	echo "intel_rapl: Step 1: Test failed"
	exit 1
else
	echo "intel_rapl: Step 1: Test passed"
fi

echo "Forcing throttle again "
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_power_limit=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_power_limit_uw)
	echo "current state " ${curr_power_limit}
	if [ $curr_power_limit -le $rapl_min_power ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_power_limit -gt $rapl_min_power ]; then
	echo "intel_rapl: Step 0: Test failed"
	exit 1
else
	echo "intel_rapl: Step 0: Test passed"
fi

echo "Removing throttle in one step"
echo 0 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_power_limit=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_power_limit_uw)
	echo "current state " ${curr_power_limit}
	if [ $curr_power_limit -ge $rapl_max_power ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_power_limit -lt $rapl_max_power ]; then
	echo "intel_rapl: Step 2: Test failed"
	exit 1
else
	echo "intel_rapl: Step 2: Test passed"
fi

