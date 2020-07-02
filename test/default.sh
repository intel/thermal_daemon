#!/bin/bash

CONF_FILE="/var/run/thermald/thermal-conf.xml.auto"

echo "Executing test : default CPU cooling"
cp default.xml $CONF_FILE
dbus-send --system --dest=org.freedesktop.thermald /org/freedesktop/thermald org.freedesktop.thermald.Reinit
sleep 5

THD0_ZONE=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:x86_pkg_temp | sed 's/\/type.*//')
#rapl_max_power=$(cat /sys/class/powercap/intel-rapl/intel-rapl\:0/constraint_0_max_power_uw)
#rapl_min_power=$(expr $rapl_max_power / 2)

rapl_max_power=25000000
rapl_min_power=10000000

echo "rapl_min_power:" $rapl_min_power
echo "rapl_max_power:" $rapl_max_power

min_perf_pct=50
max_perf_pct=100
echo "min_perf_pct:" $min_perf_pct
echo "max_perf_pct:" $max_perf_pct

CDEV_POWERCLAMP=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:intel_powerclamp | sed 's/\/type.*//')
CDEV_PROCESSOR=$(grep -r . /sys/class/thermal/* 2>/tmp/err.txt | grep  type:Processor | head -1 | sed 's/\/type.*//')

cat ${THD0_ZONE}/temp
sleep 2

for i in {0..1}
do

echo "Forcing to throttle for step" $i
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


COUNTER=0
while [  $COUNTER -lt 20 ]; do
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


max_state=$(cat ${CDEV_POWERCLAMP}/max_state)
max_state=$(expr $max_state / 2)
echo $max_state 
COUNTER=0
while [  $COUNTER -lt 20 ]; do
	cur_state=$(cat ${CDEV_POWERCLAMP}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -ge $max_state ]; then
		echo "Reached max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done

if [ $cur_state -lt $max_state ]; then
	echo "powerclamp: Step 0: Test failed"
	exit 1
else
	echo "powerclamp: Step 0: Test passed"
fi


max_state_processor=3
echo $max_state_processor 
COUNTER=0
while [  $COUNTER -lt 20 ]; do
	cur_state=$(cat ${CDEV_PROCESSOR}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -ge $max_state_processor ]; then
		echo "Reached max State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done

if [ $cur_state -lt $max_state_processor ]; then
	echo "processor: Step 0: Test failed"
	exit 1
else
	echo "processor: Step 0: Test passed"
fi

if [ $i -eq 0 ]; then
echo "Removing throttle slowly stepwise"
echo 69000 > ${THD0_ZONE}/emul_temp
else
echo "Removing throttle in one shot"
echo 0 > ${THD0_ZONE}/emul_temp
fi

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	cur_state=$(cat ${CDEV_PROCESSOR}/cur_state)
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

COUNTER=0
while [  $COUNTER -lt 20 ]; do
	cur_state=$(cat ${CDEV_POWERCLAMP}/cur_state)
	echo "current state " ${cur_state}
	if [ $cur_state -le  0 ]; then
		echo "Reached min State"
		break
	fi
	sleep 5
	let COUNTER=COUNTER+1 
done
if [ $cur_state -gt -1 ]; then
	echo "powerclamp: Step 0: Test failed"
	exit 1
else
	echo "powerclamp: Step 0: Test passed"
fi

COUNTER=0
while [  $COUNTER -lt 20 ]; do
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

done
