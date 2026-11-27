Manual tests on a real machine, driving the test kernel module through
the kernel's thermal emulation (emul_temp). They need root, change the
installed configuration (/etc/thermald/thermal-conf.xml.auto) and the
real power limits, and a kernel with CONFIG_THERMAL_EMULATION=y.

Most of what they check is covered automatically, without root or real
hardware, by the VM regression suite (test/regression, see
test/README.md).

1. Build and load the test module (test/module):

	make -C ../../module
	sudo insmod ../../module/thermald_test_kern_module.ko

   exec_config_tests.sh does both itself.

2. Start thermald without --adaptive, then run from this directory:

	sudo ./exec_config_tests.sh     XML configs test1.xml .. test7.xml
	sudo ./run_all_tests.sh         also rapl, intel_pstate, powerclamp,
	                                processor and default cooling
