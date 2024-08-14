Use man pages to check command line arguments in configuration:
	man thermald
	man thermal-conf.xml

Prerequisites:
	Kernel
		Prefers kernel with
			Intel RAPL power capping driver : Available from Linux kernel 3.13.rc1
			Intel P State driver (Available in Linux kernel stable release)
			Intel Power clamp driver (Available in Linux kernel stable release)
			Intel INT340X drivers
			Intel RAPL-mmio power capping driver: Available from 5.3-rc1

Companion tools
	ThermalMonitor
		Graphical front end for monitoring and control.
		Source code is as part of tools folder in this git repository.
	dptfxtract
		Download from: https://github.com/intel/dptfxtract
		This generates configuration files for thermald on some systems.

Building and executing on Fedora
1.
Install

	yum install automake
	yum install autoconf-archive
	yum install gcc
	yum install gcc-c++
	yum install glib-devel
	yum install libxml2-devel
	yum install gtk-doc
	yum install upower-devel
	yum install libevdev-devel

Replace yum with dnf for later Fedora versions.

2
Build

	./autogen.sh prefix=/
	make
	sudo make install

The prefix value depends on the distribution version.
This can be "/" or "/usr". So please check existing
path of thermald install, if present to update and
add appropriate prefix.

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
	sudo apt install autoconf
	sudo apt install autoconf-archive
	sudo apt install g++
	sudo apt install libglib2.0-dev
	sudo apt install libxml2-dev
	sudo apt install gtk-doc-tools
	sudo apt install libupower-glib-dev
	sudo apt install libevdev-dev

2
Build

	./autogen.sh prefix=/
	make
	sudo make install

(It will give error for systemd configuration, but ignore)

3.
If using systemd, use
- start service
	sudo systemctl start thermald.service
- Get status
	sudo systemctl status thermald.service
- Stop service
	sudo systemctl stop thermald.service

Building and executing on openSUSE
1.
Install
	zypper in automake
	zypper in gcc
	zypper in gcc-c++
	zypper in glib2-devel
	zypper in libxml2-devel
	zypper in automake autoconf-archive
	zypper in gtk-doc
	zypper in libupower-glib-devel
	zypper in libevdev-devel

For build, follow the same procedure as Fedora.

-------------------------------------------

Releases

Release 2.5.8
- Lunar Lake support
- Arrow Lake support
- Remove coverity errors to minimum
- Add a script to plot temperature and trips from debug log
- Deprecate modem/KBL-G support
- Remove dbus-glib-devel as requirment

Release 2.5.7
- Remove dependency on lzma libs
- Fix remaining issues with  GDBUS transition
- Seg fault when no config file for the first time

Release 2.5.6
- Fix crash with GDBus port

Release 2.5.5
- Use GDBus for dbus
- Handle single trip to solve thermal shutdown issue for an Alder Lake desktop

Release 2.5.4
- Android support
- Workarounds for missing conditions/tables

Release 2.5.3
- Support Meteor Lake

Release 2.5.2
- Support Alder Lake N
- Support ITMT version 2, which is used in some Raptor Lake systems

Release 2.5.1
- Static analysis fixes
- Missing init, which causes skipping of conditions in a Dell system

Release 2.5
- Support of new thermal table for Alder Lake
- Add Raptor Lake in the list

Release 2.4.9
- Fix performance issues for Dell Latitude 5421
- Fix performance issues for Dell Latitude 7320/7420
(Depend on kernel patch "thermal: int340x: Update OS policy capability handshake")
- Adaptive improvements from Benzea
- Thermal Monitor fixes and cosmetic updates
- Documentation updates from Colin King
- Static analysis fixes from Benzea
- Fix test for compressed data vaults

Release 2.4.8
-Fix Ideapad thermal shutdown issue #328

Release 2.4.7
- Fix AC/DC power limit issue in some HP TigerLake systems
- Regression fix for RAPL MSR usage in xml config file
- Added Japer Lake and Alder Lake CPU models
- Debug scripts for log collection to upload

Release 2.4.6
- Fix for Ubuntu bug 1930422

Release 2.4.5
- Address low performance with Dell Latitude 5420
with the latest BIOS

Release 2.4.4
- Address low performance with Dell Latitude 5420

Release 2.4.3
- Allow --ingore-cpuid-check to use with --adaptive option

Release 2.4.2
- Issue with Dell Latitude 7400. Fix for issue #291

Release 2.4.1
- Minor change for Dell XPS 13 with Tiger Lake.

Release 2.4
- Support for Rocket Lake and Commet Lake CPU model
- Tiger Lake DPTF tables support
- CPU stuck at low frequency on two models (issue 280)
- Changes related to PID and exit codes

Release 2.3
- Merged changes from mjg59 for adaptive
- Requires Linux kernel version 5.8 or later
- By default tries --adaptive and fallback to old style
- At least some level of success to use adaptive option on:
(not expected to be on par with Windows DPTF)
Dell XPS 13 9360
Dell XPS 13 9370
Dell XPS 13 9380
Dell XPS 13 7390 2-in-1
Dell Insperion_7386
HP Spectre x360 Convertible 15-ch0xx
HP ZBook 15 G5
Lenovo Thinkpad T480

- thermald will not run on Lenovo platforms with lap mode sysfs entry

Release 2.2
- Ignore PPCC power limits when max and min power is same
- Regression in cpufreq cooling device causing min state to get stuck

Release 2.1
- Workaround for invalid PPCC power limits
- Reduce polling for power when PPCC is not present

Release 2.0
- Tiger Lake support
- PL1 limit setting from PPCC as is
- Optimize multi step, multi zone control
- Add new tag for product identification "product_sku"

Release 1.9.1
- Remove default CPU temperature compensation for auto generated configuration from dptfxtract
- Minor Android build warnings

Release 1.9
- The major change in this version is the active power limits adjustment.
This will be useful to improve performance on some newer platform. But
this will lead to increase in CPU and other temperatures. Hence this
is important to run dptfxtract version 1.4.1 tool to get performance
sensitive thermal limits (https://github.com/intel/dptfxtract/commits/v1.4.1).
If the default configuration picked up by thermald is not optimal, user
can select other less aggressive configuration. Refer to the README here
https://github.com/intel/dptfxtract/blob/master/README.txt

This power limit adjustment depends on some kernel changes released with
kernel version v5.3-rc1. For older kernel release run thermald with
--workaround-enabled
But this will depend on /dev/mem access, which means that platforms with
secure boot must update to newer kernels.

- TCC offset limits
As reported in some forums that some platforms have issue with high TCC
offset settings. Under some special condition this offset is adjusted,
but that currently needs msr module loaded to get MSR access
from user space. I have submitted a patch to have this exported via sysfs
for v5.4+ kernel.

- To disable all the above performance optimization, use --disable-active-power.
Since Linux Thermal Daemon implementation doesn't have capability to match
Intel® Dynamic Platform and Thermal Framework (DPTF) implementation on other
Operating systems, this option is very important if the user is experiencing
thermal issues. If there is some OEM/manufactures have issue with this
implementation, please get back to me for blacklist of platforms.

- Added support for Ice Lake platform

- ThermalMonitor
Cleaned up the plots, so that only active sensors and trips gets plotted.

Release 1.8
- Support of KBL-G with discrete GPU
- Fast removal of any cooling action which was applied once
temperature is normal
- Android support
- Add Hot trip point, which when reached just calls "suspend"
- Adding new tag "DependsOn" which enable/disable trip based on some other trip
- Polling interval can be configured via thermal xml config
- Per trip PID control
- Simplify RAPL cooling device

Release 1.7.2
- Workwround for platform with invalid thermal table
- Error printing for RAPL constraint sysfs read on failure
- thermal-conf.xml.auto  can be read from /etc/thermald, which allows user to modify
generated thermal-conf.xml from /var/run/thermald and copy to /etc/thermald

Release 1.7.1
- Removed dptfxtract binary as there is an issue
in packaging this with GPL source for distributions

Release 1.7
- Add GeminiLake
- Add dptfxtract tool, which converts DPTF tables to thermald tables using best effort
- Changes to accommodate dptfxtract tool conversions
- Better facility to configure fan controls
- PID control optimization
- Fix powerlimit write errors because of bad FW settings of power limits
- More restrictive compile options and warnings as errors
- Improve logging
- Android build fixes

Release 1.6
- Add Kabylake and missing Broadwell CPU model
- Removed deprecated modules
- Added passive trip between critical and max, to allow fan to take control first
- Fixed clash when multiple zones and trips controlling same cdev

1.5.4
- Use Processor thermal device in lieu of CPU zone when present
- Haswell/Skylake PCH sensor
- Fix regression in LCD/Backlight path

Release 1.5.3
- PCH sensor support

Release 1.5.2
- Security bug for bios lock fix

Release 1.5.1
- Regression fix for the default config file location

Release 1.5
- Default warning level increase so that doesn't print much in logs
- Add new feature to set specific target state on reaching a threshold,
this allows multiple thresholds (trips)
- Android update for build
- Additional backlight devices
- New option to specify config file via command line
- Prevent adding cooling device in /etc via dbus
- Whitelist of processor models, to avoid startup on server platforms

Release 1.4.3
- One new dbus message to get temp
- Fixes to prevent warnings

Release 1.4
- Extension of DBUS I/F for developing Monitoring and Control GUI
- Added exampled to thermal-conf man page
- Support INT340X class of thermal control introduced in kernel 4.0
- Reinit without restart thermald to load new parameters like new control temperature
- Fix indexes when Linux thermal sysfs doesn't have contiguous zone numbering
- Support for new Intel SoC platforms
- Introduce back-light control as the Linux back light cooling device is removed
- Restore modified passive trip points in thermal zones on exit
- Virtual Sensor definition
- Fix loop when uevents floods the system
- Error message removal for rapl sysfs traversal
- Coverity error

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
- Allow user to change the max temperature via dbus message
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
- By default only will use DTS core temperature and p/turbo/t states only
- All the previous controls based on the zones/cdevs and XML configuration is only done, when activated via command line
- The set points are calculated and stored in a config file when it hits thermal threshold and adjusted based
on slope and angular increments to dynamically adjust set point


Version 0.2
- Define XML interface to set configuration data. Refer to thermal-conf.xml. This allows overriding buggy Bios thermal comfiguration and also allows extending the capability.
- Use platform DMI UUID to index into configuration data. If there is no UUID match, falls back to thermal sysfs
- Terminate interface
- Takes over control from kernel thermal processing
- Clean up of classes.


Version 0.1
- Dbus interface to set preferred policy: "performance", "quiet/power", "disabled"
- Defines a C++ classes for zones, cooling devices, trip points, thermal engine
- Methods can be overridden in a custom class to modify default behaviour
- Read thermal zone and cooling devices, trip points etc,
- Read temperature via netlink notification or via polling configurable via command line
- Once a trip point is crossed, activate the associate cooling devices. Start with min tstate to max tstate for each cooling device.
- Based on active or passive settings it decides the cooling devices

