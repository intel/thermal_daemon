#ifndef __ACPI_ACPI_THERMAL_H
#define __ACPI_ACPI_THERMAL_H

#include <asm/ioctl.h>                                                   

#define ACPI_THERMAL_MAGIC 's'                                                  

#define ACPI_THERMAL_GET_TRT_LEN	_IOR(ACPI_THERMAL_MAGIC, 1, unsigned long)
#define ACPI_THERMAL_GET_ART_LEN	_IOR(ACPI_THERMAL_MAGIC, 2, unsigned long)
#define ACPI_THERMAL_GET_TRT_COUNT	_IOR(ACPI_THERMAL_MAGIC, 3, unsigned long)
#define ACPI_THERMAL_GET_ART_COUNT	_IOR(ACPI_THERMAL_MAGIC, 4, unsigned long)

#define ACPI_THERMAL_GET_TRT	_IOR(ACPI_THERMAL_MAGIC, 5, unsigned long)
#define ACPI_THERMAL_GET_ART	_IOR(ACPI_THERMAL_MAGIC, 6, unsigned long)

#ifndef __KERNEL__
#define u64 unsigned long long
#endif

union art_object {
	struct {
		char source_device[8]; /* ACPI single name */
		char target_device[8]; /* ACPI single name */
		u64 weight;
		u64 ac0_max_level;
		u64 ac1_max_level;
		u64 ac2_max_level;
		u64 ac3_max_level;
		u64 ac4_max_level;
		u64 ac5_max_level;
		u64 ac6_max_level;
		u64 ac7_max_level;
		u64 ac8_max_level;
		u64 ac9_max_level;
	}acpi_art_entry;
	u64 __data[13];
};

union trt_object {
	struct {
		char source_device[8]; /* ACPI single name */
		char target_device[8]; /* ACPI single name */
		u64 influence;
		u64 sample_period;
		u64 reserved[4];
	} acpi_trt_entry;
	u64 __data[8];
};

#ifdef __KERNEL__
int acpi_thermal_rel_misc_device_add(acpi_handle handle);
int acpi_thermal_rel_misc_device_remove(acpi_handle handle);
#endif

#endif /* __ACPI_ACPI_THERMAL_H */
