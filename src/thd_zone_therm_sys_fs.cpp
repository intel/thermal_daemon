/*
 * thd_zone_therm_sys_fs.cpp: thermal zone class implementation
 *	for thermal sysfs
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "thd_zone_therm_sys_fs.h"
#include "thd_engine.h"
#include <stdlib.h>

cthd_sysfs_zone::cthd_sysfs_zone(int count, std::string path) :
		cthd_zone(count, path), trip_point_cnt(0) {

	std::stringstream tc_type_dev;
	tc_type_dev << index << "/type";

	thd_log_debug("Thermal Zone look for %s\n", tc_type_dev.str().c_str());

	if (zone_sysfs.exists(tc_type_dev.str())) {
		zone_sysfs.read(tc_type_dev.str(), type_str);
	}

	thd_log_debug("Thermal Zone %d:%s\n", index, type_str.c_str());
}

cthd_sysfs_zone::~cthd_sysfs_zone() {
	std::stringstream trip_sysfs;
	trip_sysfs << index << "/" << "trip_point_";

	for (unsigned int i = 0; i < initial_trip_values.size(); ++i) {
		std::stringstream temp_stream;
		temp_stream << trip_sysfs.str() << i << "_temp";
		if (initial_trip_values[i] >= 0
				&& zone_sysfs.exists(temp_stream.str())) {
			zone_sysfs.write(temp_stream.str(), initial_trip_values[i]);
		}
	}
}

int cthd_sysfs_zone::zone_bind_sensors() {
	cthd_sensor *sensor;

	sensor = thd_engine->search_sensor(type_str);
	if (sensor) {
		bind_sensor(sensor);
	} else
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_sysfs_zone::read_trip_points() {

	// Gather all trip points
	std::stringstream trip_sysfs;
	trip_sysfs << index << "/" << "trip_point_";
	for (int i = 0; i < max_trip_points; ++i) {
		std::stringstream type_stream;
		std::stringstream temp_stream;
		std::stringstream hist_stream;
		std::string _type_str;
		std::string _temp_str;
		std::string _hist_str;
		trip_point_type_t trip_type;
		int temp = 0, hyst = 1;
		mode_t mode = 0;
		cthd_sensor *sensor;
		bool wr_mode = false;

		type_stream << trip_sysfs.str() << i << "_type";
		if (zone_sysfs.exists(type_stream.str())) {
			zone_sysfs.read(type_stream.str(), _type_str);
			thd_log_debug("read_trip_points %s:%s \n",
					type_stream.str().c_str(), _type_str.c_str());
		}
		temp_stream << trip_sysfs.str() << i << "_temp";
		if (zone_sysfs.exists(temp_stream.str())) {
			mode = zone_sysfs.get_mode(temp_stream.str());
			zone_sysfs.read(temp_stream.str(), _temp_str);
			std::istringstream(_temp_str) >> temp;
			thd_log_debug("read_trip_points %s:%s \n",
					temp_stream.str().c_str(), _temp_str.c_str());
		}

		hist_stream << trip_sysfs.str() << i << "_hyst";
		if (zone_sysfs.exists(hist_stream.str())) {
			zone_sysfs.read(hist_stream.str(), _hist_str);
			std::istringstream(_hist_str) >> hyst;
			if (hyst < 1000 || hyst > 5000)
				hyst = 1000;
			thd_log_debug("read_trip_points %s:%s \n",
					hist_stream.str().c_str(), _hist_str.c_str());
		}

		if (_type_str == "critical")
			trip_type = CRITICAL;
		else if (_type_str == "hot")
			trip_type = MAX;
		else if (_type_str == "active")
			trip_type = ACTIVE;
		else if (_type_str == "passive")
			trip_type = PASSIVE;
		else
			trip_type = INVALID_TRIP_TYPE;

		sensor = thd_engine->search_sensor(type_str);
		if (sensor && (mode & S_IWUSR)) {
			sensor->set_async_capable(true);
			wr_mode = true;
			initial_trip_values.push_back(temp);
		} else
			initial_trip_values.push_back(-1);

		if (sensor && temp > 0 && trip_type != INVALID_TRIP_TYPE && !wr_mode) {

			cthd_trip_point trip_pt(trip_point_cnt, trip_type, temp, hyst,
					index, sensor->get_index());
			trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
			trip_points.push_back(trip_pt);
			++trip_point_cnt;
		}
	}

	thd_log_debug("read_trip_points Added from sysfs %d trips \n", trip_point_cnt);

	if (!trip_point_cnt) {
		cthd_sensor *sensor;

		sensor = thd_engine->search_sensor(type_str);
		if (!sensor)
			return THD_ERROR;

		cthd_trip_point trip_pt(0, PASSIVE, INT32_MAX, 0, index, sensor->get_index());
		trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
		trip_points.push_back(trip_pt);
		++trip_point_cnt;
		thd_log_debug("Added one default trip\n");
	}

	return THD_SUCCESS;
}

int cthd_sysfs_zone::read_cdev_trip_points() {
	thd_log_debug(" >> read_cdev_trip_points for \n");

	// Gather all Cdevs
	// Gather all trip points
	std::stringstream cdev_sysfs;
	cdev_sysfs << index << "/" << "cdev";
	for (int i = 0; i < max_cool_devs; ++i) {
		std::stringstream trip_pt_stream, cdev_stream;
		std::string trip_pt_str;
		int trip_cnt = -1;
		char buf[51], *ptr;
		trip_pt_stream << cdev_sysfs.str() << i << "_trip_point";

		if (zone_sysfs.exists(trip_pt_stream.str())) {
			zone_sysfs.read(trip_pt_stream.str(), trip_pt_str);
			std::istringstream(trip_pt_str) >> trip_cnt;
		} else
			continue;
		thd_log_debug("cdev trip point: %s contains %d\n", trip_pt_str.c_str(),
				trip_cnt);
		cdev_stream << cdev_sysfs.str() << i;
		if (zone_sysfs.exists(cdev_stream.str())) {
			thd_log_debug("cdev%d present\n", i);
			int ret = zone_sysfs.read_symbolic_link_value(cdev_stream.str(),
					buf, sizeof(buf) - 1);
			if (ret == 0) {
				ptr = strstr(buf, "cooling_device");
				if (ptr) {
					ptr += strlen("cooling_device");
					thd_log_debug("symbolic name %s:%s\n", buf, ptr);
					if (trip_cnt >= 0 && trip_cnt < trip_point_cnt) {
						trip_points[trip_cnt].thd_trip_point_add_cdev_index(
								atoi(ptr), cthd_trip_point::default_influence);
						zone_cdev_set_binded();
					} else {
						thd_log_debug("Invalid trip_cnt\n");
					}
				}
			}
		}
	}
	thd_log_debug(
			"cthd_sysfs_zone::read_cdev_trip_points: ZONE bound to CDEV status %d \n",
			zone_cdev_binded_status);

	return THD_SUCCESS;
}
