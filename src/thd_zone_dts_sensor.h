/*
 * thd_zone_dts_sensor.h: thermal engine DTS sensor class interface
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
 * This implementation allows to use core temperature interface.
 */

#ifndef THD_ZONE_DTS_SENSOR_H_
#define THD_ZONE_DTS_SENSOR_H_

#include "thd_zone_dts.h"

class cthd_zone_dts_sensor: public cthd_zone_dts
{
private:
	int index;
	unsigned int read_cpu_mask();
	bool conf_present;

public:
	cthd_zone_dts_sensor(int count, int _index, std::string path);
	int read_trip_points();
	unsigned int read_zone_temp();

};



#endif /* THD_ZONE_DTS_SENSOR_H_ */
