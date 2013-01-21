/*
 * thd_engine_dts.h: thermal engine dts class interface
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
 */

#ifndef THD_ENGINE_DTS_H_
#define THD_ENGINE_DTS_H_


#include "thd_engine.h"

class cthd_engine_dts: public cthd_engine
{
private:
	csys_fs 				thd_sysfs;

public:
	cthd_engine_dts() : thd_sysfs("/sys/devices/platform/coretemp.0/") {}
	int read_thermal_zones();
	int read_cooling_devices();
};


#endif /* THD_ENGINE_DTS_H_ */
