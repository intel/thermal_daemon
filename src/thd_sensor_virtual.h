/*
 * thd_sensor_virtual.h: thermal sensor virtual class interface
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

#ifndef THD_SENSOR_VIRTUAL_H_
#define THD_SENSOR_VIRTUAL_H_

#include "thd_sensor.h"

class cthd_sensor_virtual: public cthd_sensor {
private:
	cthd_sensor *link_sensor;
	std::string link_type_str;
	double multiplier;
	double offset;

public:
	cthd_sensor_virtual(int _index, std::string _type_str,
			std::string _link_type_str, double multiplier, double offset);
	~cthd_sensor_virtual();
	int sensor_update();
	unsigned int read_temperature();
	virtual void sensor_dump() {
		if (link_sensor)
			thd_log_info("sensor index:%d %s virtual link %s %f %f\n", index,
					type_str.c_str(), link_sensor->get_sensor_type().c_str(),
					multiplier, offset);
	}

	int sensor_update_param(std::string new_dep_sensor, double slope, double intercept);

};

#endif /* THD_SENSOR_VIRTUAL_H_ */
