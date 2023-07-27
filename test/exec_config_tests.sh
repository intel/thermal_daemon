#!/bin/bash

echo "Make sure that intel_pstate is not started with --adaptive option"

make

insmod thermald_test_kern_module.ko

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

#Test 1: Simple association: one zone to one cooling device
# check if cdev state reach max when temp >= 40C and when
# temp < 40C state reach to 0.

echo "Executing test 1: Simple zone to cdev association"
cp test1.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:thd_test_0 | sed 's/\/type.*//')
THD0_CDEV=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  thd_cdev_0 | sed 's/\/type.*//')
echo "Current temperature for thd_test_0 temp to"
cat ${THD0_ZONE}/temp
sleep 2
echo 50000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 
cat ${THD0_CDEV}/cur_state
echo 10000 > ${THD0_ZONE}/emul_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi

# TEST 2
echo "Executing test 2: Check if influence field is respected, in picking up cdev"
cp test2.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:thd_test_0 | sed 's/\/type.*//')
THD0_CDEV0=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  thd_cdev_0 | sed 's/\/type.*//')
THD0_CDEV1=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  thd_cdev_1 | sed 's/\/type.*//')
echo "Current temperature for thd_test_0 temp to"
cat ${THD0_ZONE}/temp
sleep 2
echo 50000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp
# check the highest priority cdev picked up first
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV1}/cur_state)
	echo "current state for thd_cdev_1" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 

# pick up the next
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV0}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done

if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 

echo 10000 > ${THD0_ZONE}/emul_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV0}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 

COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV1}/cur_state)
	echo "current state for thd_cdev_1" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 

# Test 3
echo "Executing test 3: Check if sample field is respected"
echo "currently it is a visual test only"
echo "It will show Too early to act messages, gap between two ops is 15+ sec as per connfig here "

cp test3.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:thd_test_0 | sed 's/\/type.*//')
THD0_CDEV=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  thd_cdev_0 | sed 's/\/type.*//')
echo "Current temperature for thd_test_0 temp to"
cat ${THD0_ZONE}/temp
sleep 2
echo 50000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 
cat ${THD0_CDEV}/cur_state
echo 10000 > ${THD0_ZONE}/emul_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi

# Test 4
echo "Executing test 4: one cdev in multiple zones,
one zone crossed passive, make sure that the other zone
doesn't deactivate an activated cdev "

cp test4.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:thd_test_0 | sed 's/\/type.*//')
THD0_CDEV=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  thd_cdev_0 | sed 's/\/type.*//')
echo "Current temperature for thd_test_0 temp to"
cat ${THD0_ZONE}/temp
sleep 2
echo 50000 > ${THD0_ZONE}/emul_temp
echo "Emulate temp to"
cat ${THD0_ZONE}/temp

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi 
cat ${THD0_CDEV}/cur_state
echo 10000 > ${THD0_ZONE}/emul_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD0_CDEV}/cur_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi

# Test 5
echo "Executing test 5: Test case where sensor/zone/cdev are not"
echo " in thermal sysfs and still able to control"

cp test5.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD5_ZONE=/sys/kernel/thermald_test
THD5_CDEV=/sys/kernel/thermald_test
echo "Current temperature"
cat ${THD5_ZONE}/sensor_temp
sleep 2
echo 50000 > ${THD5_ZONE}/sensor_temp
echo "Emulate temp to"
cat ${THD5_ZONE}/sensor_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD5_CDEV}/control_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 10 ]; then
		echo "Reached Max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1
done

if [ $curr_state -ne 10 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi
cat ${THD5_CDEV}/control_state
echo 10000 > ${THD5_ZONE}/sensor_temp
COUNTER=0
while [  $COUNTER -lt 10 ]; do
	curr_state=$(cat ${THD5_CDEV}/control_state)
	echo "current state for thd_cdev_0" ${curr_state}
	if [ $curr_state -eq 0 ]; then
		echo "Reached Min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1
done
if [ $curr_state -ne 0 ]; then
	echo "Test Failed"
	exit 1
else
	echo "Test passed"
fi
