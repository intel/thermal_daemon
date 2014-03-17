/*
 * thd_zone_surface.h: zone interface for external surface
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef THD_ZONE_SURFACE_H_
#define THD_ZONE_SURFACE_H_

#include "thd_zone_therm_sys_fs.h"
#include "thd_engine.h"

class cthd_zone_surface: public cthd_zone {
private:
	cthd_sensor *sensor;

public:
	static const int passive_trip_temp = 50000;
	static const int passive_trip_hyst = 1000;
	static const int surface_sampling_period = 12;

	cthd_zone_surface(int count);

	int read_trip_points();
	int read_cdev_trip_points();
	int zone_bind_sensors();
};

#endif /* THD_ZONE_SURFACE_H_ */
