/*
 * thd_engine_custom.h: thermal engine custom class interface
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

#ifndef THD_ENGINE_CUSTOM_H
#define THD_ENGINE_CUSTOM_H

#include "thd_engine.h"

class cthd_engine_custom :  public cthd_engine {
private:
	bool parse_success;

public:
	~cthd_engine_custom() { }
	int read_thermal_zones();
	int read_cooling_devices();
	cthd_parse parser;

	int use_custom_engine() { return parse_success; }
};

#endif
