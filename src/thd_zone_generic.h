/*
 * thd_zone_generic.h: zone interface for xml conf
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

#ifndef THD_ZONE_GENERIC_H_
#define THD_ZONE_GENERIC_H_

#include "thd_zone.h"

class cthd_zone_generic: public cthd_zone {
private:
	int trip_point_cnt;
	int config_index;
	cthd_zone *zone;
	std::vector<cthd_sensor *> sensor_list;

public:

	cthd_zone_generic(int index, int _config_index, std::string type);

	virtual int read_trip_points();
	virtual int read_cdev_trip_points();
	virtual int zone_bind_sensors();
};

#endif /* THD_ZONE_GENERIC_H_ */
