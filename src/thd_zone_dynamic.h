/*
 * thd_zone_generic.cpp: zone implementation for xml conf
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

#ifndef THD_ZONE_DYNAMIC_H_
#define THD_ZONE_DYNAMIC_H_

#include "thd_zone.h"

class cthd_zone_dynamic: public cthd_zone {
private:
	std::string name;
	unsigned int trip_temp;
	trip_point_type_t trip_type;
	std::string sensor_name;
	std::string cdev_name;

public:

	cthd_zone_dynamic(int index, std::string _name, unsigned int _trip_temp, trip_point_type_t _trip_type,
			std::string _sensor, std::string _cdev);

	virtual int read_trip_points();
	virtual int read_cdev_trip_points();
	virtual int zone_bind_sensors();
};

#endif /* THD_ZONE_DYNAMIC_H_ */
