/*
 * thd_engine_custom.cpp: thermal engine custom class implementation
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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
 * This interface allows to overide reading of zones and cooling devices from
 * default ACPI sysfs. 
 */

#include "thd_engine_custom.h"

int cthd_engine_custom::read_thermal_zones()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_engine::read_thermal_zones();
}

int cthd_engine_custom::read_cooling_devices()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_engine::read_cooling_devices();
}

