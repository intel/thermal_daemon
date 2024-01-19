/*
 * acpi_thermal_rel_ioct.h: head file of _ART and _TRT objects
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 or later as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

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

/*
 * ACPI_THERMAL_GET_PSVT_COUNT = Number of PSVT entries
 * ACPI_THERMAL_GET_PSVT_LEN = Total return data size (PSVT count x each
 * PSVT entry size)
 * ACPI_THERMAL_GET_PSVT = Get the data as an array of psvt_objects
 */
#define ACPI_THERMAL_GET_PSVT_LEN _IOR(ACPI_THERMAL_MAGIC, 7, unsigned long)
#define ACPI_THERMAL_GET_PSVT_COUNT _IOR(ACPI_THERMAL_MAGIC, 8, unsigned long)
#define ACPI_THERMAL_GET_PSVT	_IOR(ACPI_THERMAL_MAGIC, 9, unsigned long)

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

#define ACPI_LIMIT_STR_MAX_LEN	8
#define ACPI_NR_PSVT_ELEMENTS	12

union psvt_object {
	struct {
		char source_device[8];
		char target_device[8];
		u64 priority;
		u64 sample_period;
		u64 passive_temp;
		u64 source_domain;
		u64 control_knob;
		union {
			u64 integer;
			char string[ACPI_LIMIT_STR_MAX_LEN];
		} limit;
		u64 step_size;
		u64 limit_coeff;
		u64 unlimit_coeff;
		u64 control_knob_type;
	} acpi_psvt_entry;
	u64 __data[ACPI_NR_PSVT_ELEMENTS];
};

#ifdef __KERNEL__
int acpi_thermal_rel_misc_device_add(acpi_handle handle);
int acpi_thermal_rel_misc_device_remove(acpi_handle handle);
#endif

#endif /* __ACPI_ACPI_THERMAL_H */
