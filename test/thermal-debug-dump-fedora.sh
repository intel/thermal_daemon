#!/bin/bash
echo "Installing Packages"

dnf install -y automake autoconf-archive gcc gcc-c++ \
glib-devel dbus-glib-devel libxml2-devel \
gtk-doc upower-devel libevdev-devel kernel-tools \
stress-ng acpidump

status=$?
if [ $status -eq 0 ];
then
echo "Packages installed successfully"
else
echo "Package install failed"
exit 1
fi


Day=`date +%d`
Hour=`date +%H`
Minute=`date +%M`
Second=`date +%S`
folder_name=$Day$Hour$Minute$Second
echo $folder_name
mkdir $folder_name

grep . /sys/class/powercap/intel-rapl/intel-rapl\:0/* > $folder_name/powercap_msr.txt
grep . /sys/class/powercap/intel-rapl-mmio/intel-rapl-mmio\:0/* > $folder_name/powercap_mmio.txt
grep . /sys/devices/system/cpu/cpu0/cpufreq/* > $folder_name/cpufreq.txt

acpidump -o $folder_name/acpi_dump.out

mkdir debug
cd debug


echo "Downloading latest thermald"

git clone https://github.com/intel/thermal_daemon.git
status=$?
if [ $status -eq 0 ];
then
echo "git clone successful"
else
echo "git clone failed"
exit 1
fi

cd thermal_daemon
echo "Building thermald"
./autogen.sh prefix=/
make -j8
status=$?
if [ $status -eq 0 ];
then
echo "build successful"
else
echo "build failed"
exit 1
fi

systemctl stop thermald

cd ../../$folder_name

echo "Starting thermald"
../debug/thermal_daemon/thermald --no-daemon --loglevel=debug --adaptive --ignore-cpuid-check > thermald_log.txt&
sleep 30
grep . /sys/class/powercap/intel-rapl/intel-rapl\:0/* > powercap_msr_after.txt
grep . /sys/class/powercap/intel-rapl-mmio/intel-rapl-mmio\:0/* > powercap_mmio_after.txt

echo "Executing stress-ng"
stress-ng --cpu -1 --io 4 --vm 2 --vm-bytes 128M --fork 4 --timeout 300s&
turbostat --show Core,CPU,Busy%,Bzy_MHz,TSC_MHz -o turbostat.out&
sleep 305

pkill thermald
pkill turbostat
systemctl start thermald

cd ..
echo -n "Creating archive:"
echo $folder_name.tar.gz
tar cvfz $folder_name.tar.gz $folder_name
echo -n "Attach archive to debug:"
echo $folder_name.tar.gz
