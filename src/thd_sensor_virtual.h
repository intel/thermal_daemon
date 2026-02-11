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
#include <vector>

typedef struct _link_sensor_t {
	cthd_sensor *sensor;
	double coeff;
	double offset;
	double prev_avg;
} link_sensor_t;

class cthd_sensor_virtual: public cthd_sensor {
private:
	std::vector <link_sensor_t *> link_sensors;
	double multiplier;
	double offset;

public:
	cthd_sensor_virtual(int _index, std::string _type_str,
			std::string& _link_type_str, double multiplier, double offset);
	~cthd_sensor_virtual() override;

	int add_target(std::string& _link_type_str, double coeff, double offset);

	int sensor_update();
	unsigned int read_temperature() override;
	void sensor_dump() override {
		thd_log_info("Sensor:%s \n", type_str.c_str());

		for (unsigned int i = 0; i < link_sensors.size(); ++i) {
			link_sensor_t *link_sensor = link_sensors[i];
			if (link_sensor->sensor) {
				thd_log_info("\tAdd target sensor %s, %g %g\n",
					link_sensor->sensor->get_sensor_type().c_str(), link_sensor->coeff, link_sensor->offset);
			}
		}
	}

	int sensor_update_param(const std::string& new_dep_sensor, double slope, double intercept);

};

#endif /* THD_SENSOR_VIRTUAL_H_ */
