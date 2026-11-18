This is for test on pre-production systems.

1. Insert module to create a test zone SEN2
 sudo insmod  ./thermald_test_kern_module.ko zone_name=SEN2

2.
rename test_data_vault.bin to /lib/firmware/intel/dtt/dtt_data_vault_...bin

3.
Use emul_temp to simulate temperature in the zone for SEN2
