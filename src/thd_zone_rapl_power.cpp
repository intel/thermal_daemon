/*
 * thd_zone_rapl_power.cpp: thermal zone for rapl power
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
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

#include "thd_engine_default.h"
#include "thd_sys_fs.h"
#include "thd_zone_rapl_power.h"

cthd_zone_rapl_power::cthd_zone_rapl_power(int index) :
		cthd_zone(index, "") {
	type_str = "rapl_pkg_power";
}

int cthd_zone_rapl_power::zone_bind_sensors() {

	cthd_sensor *sensor;

	sensor = thd_engine->search_sensor("rapl_pkg_power");
	if (sensor) {
		bind_sensor(sensor);
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_zone_rapl_power::read_trip_points() {
	cthd_cdev *cdev_cpu;

	cdev_cpu = thd_engine->search_cdev("rapl_controller");
	if (!cdev_cpu) {
		thd_log_info("rapl_controller, failed\n");
		return THD_ERROR;
	}

	cthd_trip_point trip_pt_passive(0, PASSIVE, 100000, 0, index,
	DEFAULT_SENSOR_ID, PARALLEL);

	trip_pt_passive.thd_trip_point_add_cdev(*cdev_cpu,
			cthd_trip_point::default_influence, thd_engine->get_poll_interval(),
			0, 0, NULL);

	trip_points.push_back(trip_pt_passive);

	thd_log_debug("cthd_zone_rapl_power::read_trip_points  OK\n");

	return THD_SUCCESS;
}

int cthd_zone_rapl_power::read_cdev_trip_points() {
	return THD_SUCCESS;
}
