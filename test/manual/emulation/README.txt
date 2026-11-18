Tests are executed using a loopback Linux driver.
Build this as a kernel module by copying
thermald_test_kern_module.c to drivers/thermal in Linux kernel source.
Add
obj-m += thermald_test_kern_module.o in drivers/thermal/Makefile

In addition enable in kernel .config
CONFIG_THERMAL_EMULATION=y

Once kernel driver is loaded using insmod/modprobe
execute exec_config_tests.sh
