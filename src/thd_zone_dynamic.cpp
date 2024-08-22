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

#include "thd_zone_dynamic.h"
#include "thd_engine.h"

cthd_zone_dynamic::cthd_zone_dynamic(int index, std::string _name,
		unsigned int _trip_temp, trip_point_type_t _trip_type, std::string _sensor, std::string _cdev) :
		cthd_zone(index, ""), name(std::move(_name)), trip_temp(_trip_temp), trip_type(_trip_type), sensor_name(
				std::move(_sensor)), cdev_name(std::move(_cdev)) {
	type_str = name;
}

int cthd_zone_dynamic::read_trip_points() {

	cthd_sensor *sensor = thd_engine->search_sensor(sensor_name);
	if (!sensor) {
		thd_log_warn("dynamic sensor: invalid sensor type\n");
		return THD_ERROR;
	}
	thd_log_info("XX index = %d\n", index);
	cthd_trip_point trip_pt(0, trip_type, trip_temp, 0, index,
			sensor->get_index());

	trip_points.push_back(trip_pt);

	cthd_cdev *cdev = thd_engine->search_cdev(cdev_name);
	if (cdev) {
		trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence);
		zone_cdev_set_binded();
	} else
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_zone_dynamic::read_cdev_trip_points() {
	return 0;
}

int cthd_zone_dynamic::zone_bind_sensors() {

	cthd_sensor *sensor = thd_engine->search_sensor(sensor_name);
	if (!sensor) {
		thd_log_warn("dynamic sensor: invalid sensor type\n");
		return THD_ERROR;
	}
	bind_sensor(sensor);

	return THD_SUCCESS;
}

