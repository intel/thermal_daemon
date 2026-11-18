#!/bin/bash
echo "This test will exit by printing error if there are no DPTF tables found"
echo "Installing Packages"
apt install -y git autoconf-archive g++ libglib2.0-dev libdbus-1-dev \
libdbus-glib-1-dev libxml2-dev gtk-doc-tools libupower-glib-dev \
libevdev-dev
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
../debug/thermal_daemon/thermald --no-daemon --loglevel=debug --adaptive --ignore-cpuid-check --test-mode
