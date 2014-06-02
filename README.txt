Use man pages to check command line arguments in configuration:
	man thermald
	man thermal-conf.xml

Prerequisites:
	Kernel
		Prefers kernel with
			Intel RAPL power capping driver : Available from Linux kernel 3.13.rc1
			Intel P State driver (Available in Linux kernel stable release)
			Intel Power clamp driver (Available in Linux kernel stable release)

		CONFIG_X86_MSR, so that x86 MSR can be read/write from user space to control RAPL if no RAPL powecap class driver is not present.

	Default
		If none of the above available cpufreq to control P states.

Building and executing on Fedora
1.
Install

	yum install automake
	yum install gcc
	yum install gcc-c++
	yum install glib-devel
	yum install dbus-glib-devel
	yum install libxml2-devel

2
Build

	./autogen.sh
	 ./configure prefix=/usr
	make
	sudo make install

3
- start service
	sudo systemctl start thermald.service
- Get status
	sudo systemctl status thermald.service
- Stop service
	sudo systemctl stop thermald.service

4. Terminate using DBUS I/F
	sudo test/test_pref.sh
		and select "TERMINATE" choice.



Building on Ubuntu
1. Install
	sudo apt-get install autoconf
	sudo apt-get install g++
	sudo apt-get install libglib2.0-dev
	sudo apt-get install libdbus-1-dev
	sudo apt-get install libdbus-glib-1-dev
	sudo apt-get install libxml2-dev

2
Build

	./autogen.sh
	 ./configure prefix=/usr
	make
	sudo make install
(It will give error for systemd configuration, but ignore)
cp data/thermald.conf /etc/init/
3.
Use "sudo start thermald" to start
Use "sudo stop thermald" to stop

-------------------------------------------

Releases

Release 1.3
- Auto creation of configuration based on ACPI thermal relationship table
- Default CPU bound load check for unbinded thermal sensors

Release 1.2
- Several fixes for Klocworks and Coverity scans (0 issues remaining)
- Baytrail RAPL support as this doesn't have max power limit value

Release 1.1
- Use powercap Intel RAPL driver
- Use skin temperature sensor by default if available
- Specify thermal relationship
- Clean up for MSR related controls as up stream kernel driver are capable now
- Override capability of thermal sysfs for a specific sensor or zone
- Friendly to new thermal sysfs

Release 1.04
- Android and chrome os integration
- Minor fixes for buggy max temp

Release 1.03
- Allow negative step increments while configuring via XML
- Use powercap RAPL driver I/F
- Additional cpuids in the list
- Add man page with details of usage
- Added P state turbo on/off

Release 1.02
- Allow user to change the max temperarure via dbus message
- Allow user to change the cooling method order via an XML configuration
- Upstart fixes
- Valgrind and zero warnings on build

Release 1.01
- Implement RAPL using MSRs.
- User can configure cooling device order via XML config file
- Fix sensor path configuration for thermal-conf.xml, so that user cn specify custom sensor paths
- Use CPU max scaling frequency to control CPU Frequencies
- RPM generation scripts
- Build and formatting fixes from Alexander Bersenev


Release 1.0
- Tested on multiple platforms
- Using PID

version 0.9
- Replaced netlink with uevents
- Fix issue with pre-configured thermal data to control daemon
- Use pthreads

version 0.8
- Fix RAPL PATH, which is submitted upstream
- Handle case when there is no MSR access from user mode
- Allow non Intel CPUs

version 0.7
- Conditional per cpu control
- Family id check
- If no max use offset from critical temperature
- Switch to hwmon if there is no coretemp
- Error handling if MSR support is not enabled in kernel
- Code clean up and comments

Version 0.6
- Use Intel P state driver to control P states
- Use RAPL cooling device
- Fix valgrind reported errors and cleanup
- Add document

Version 0.5
- License update to GPL v2 or later
- Change dbus session bus to system
- Load thermal-conf.xml data if exact UUID  match

Version 0.4
- Added power clamp driver interface
- Added per cpu controls by trying to calibrate in the background to learn sensor cpu relationship
- Optimized p states and turbo states and cleaned up
- systemd and service start stop interface

Version 0.3
- Added P states t states turbo states as the cooling methods
- No longer depend on any thermal sysfs, zone cooling device by default
- Uses DTS core temperature and p/turbo/t states to cool system
- By default only will use DTS core temerature and p/turbo/t states only
- All the previous controls based on the zones/cdevs and XML configuration is only done, when activated via command line
- The set points are calculated and stored in a config file when it hits thermal threshold and adjusted based
on slope and angular increments to dynamically adjust set point


Version 0.2
- Define XML interface to set configuration data. Refere to thermal-conf.xml. This allows to overide buggy Bios thermal comfiguration and also allows to extend the capability.
- Use platform DMI UUID to index into configuration data. If there is no UUID match, falls back to thermal sysfs
- Terminate interface
- Takes over control from kernel thermal processing
- Clean up of classes.


Version 0.1
- Dbus interface to set preferred policy: "performance", "quiet/power", "disabled"
- Defines a C++ classes for zones, cooling devices, trip points, thermal engine
- Methods can be overridden in a custom class to modify default behaviour
- Read thermal zone and cooling devices, trip points etc,
- Read temprature via netlink notification or via polling configurable via command line
- Once a trip point is crossed, activate the associate cooling devices. Start with min tstate to max tstate for each cooling device.
- Based on active or passive settings it decides the cooling devices

