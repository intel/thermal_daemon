1.
Install
	automake
	gcc
	gcc-c++
	glib-devel
	dbus-glib-devel	
	libxml2-devel
2
Build

	autoreconf --install
	 ./configure
	make prefix=/var/tmp
	(prefix can be removed, it will require permission to copy to usr/local/bin and other folders)
	make install 
	(If desired)

3: What it is doing?
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

To be done:
- Change some pointers to references
- PID auto calibration for Kp, Ki, Kd.
- ??

