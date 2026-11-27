Reproducing another machine's adaptive (DTT) tables locally, on a
pre-production or development system.

1. Create a test zone named like the sensor the tables use, e.g. SEN2:

	make -C ../../module
	sudo insmod ../../module/thermald_test_kern_module.ko zone_name=SEN2

2. Copy the data vault to /lib/firmware/intel/dtt/dtt_data_vault_<...>.bin
   (thermald logs the exact file name it looks for), e.g. from
   test_data_vault.bin here.

3. Use emul_temp of the SEN2 zone to simulate its temperature.

gddv_tests/ has a JSON to data vault generator (gddv_test_gen.py) and
sample inputs. The automated tests build vaults with
test/regression/lib/gddv.py instead, and can run real data vaults from
a private directory (test/README.md, "Real data vaults").
