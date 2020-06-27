#!/bin/bash

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

echo "Executing test : Test acpi processor cooling"
cp processor_acpi.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:x86_pkg_temp | sed 's/\/type.*//')
CDEV=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:Processor | head -1 | sed 's/\/type.*//')

cat ${THD0_ZONE}/temp
sleep 2
echo "Forcing to throttle"
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

max_state=3
echo $max_state 
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	cur_state=$(cat ${CDEV}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -ge $max_state ]; then
		echo "Reached max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done

if [ $cur_state -lt $max_state ]; then
	echo "processor: Step 0: Test failed"
	exit 1
else
	echo "processor: Step 0: Test passed"
fi

echo "Removing throttle slowly stepwise"
echo 69000 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	cur_state=$(cat ${CDEV}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -le  0 ]; then
		echo "Reached min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $cur_state -gt 0 ]; then
	echo "processor: Step 0: Test failed"
	exit 1
else
	echo "processor: Step 0: Test passed"
fi



echo "Forcing throttle again "
echo 70000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	cur_state=$(cat ${CDEV}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -ge $max_state ]; then
		echo "Reached max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $cur_state -lt $max_state ]; then
	echo "processor: Step 1: Test failed"
	exit 1
else
	echo "processor: Step 1: Test passed"
fi

echo "Removing throttle in one step"
echo 0 > ${THD0_ZONE}/emul_temp

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	cur_state=$(cat ${CDEV}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -le  0 ]; then
		echo "Reached min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done

if [ $cur_state -gt 0 ]; then
	echo "processor: Step 2: Test failed"
	exit 1
else
	echo "processor: Step 2: Test passed"
fi
