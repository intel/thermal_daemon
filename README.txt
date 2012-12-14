1.
Install
	automake
	gcc
	gcc-c++
	glib-devel
	dbus-glib-devel	
2
Build

	autoreconf --install
	 ./configure
	make prefix=/var/tmp


3: What it is doing?

- Dbus interface to set preferred policy: "performance", "quiet/power", "disabled"
- Defines a C++ classes for zones, cooling devices, trip points, thermal engine
- Methods can be overridden in a custom class to modify default behaviour
- Read thermal zone and cooling devices, trip points etc,
- Read temprature via netlink notification or via polling configurable via command line
- Once a trip point is crossed, activate the associate cooling devices. Start with min tstate to max tstate for each cooling device.
- Based on active or passive settings it decides the cooling devices

To be done:
- Based on platform from DMI, allow overiding of default trip points and cooling devices
- Based on some threshold switch to active cooling even if we are in passive cooling
- Experiment and adjust ultrabook ivy-bridge trip points for passive cooling
- Use PID control from tmon
- ???


