/*
 * thd_engine_therm_sys_fs.h: thermal engine class implementation
 *  for acpi style	sysfs control.
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
 *
 *
 * Author Name <Srinivas.Pandruvada@linux.intel.com>
 *
 */

#ifndef THD_ENGINE_THERM_SYSFS_H_
#define THD_ENGINE_THERM_SYSFS_H_

#include "thd_engine.h"

class cthd_engine_therm_sysfs: public cthd_engine
{
private:
	csys_fs thd_sysfs;
	bool parser_init_done;

	void parser_init();
	int read_xml_thermal_zones();
	int read_xml_cooling_device();

public:
	cthd_engine_therm_sysfs();
	int read_thermal_zones();
	int read_cooling_devices();
};


#endif /* THD_ENGINE_ZONE_CONTROL_H_ */
