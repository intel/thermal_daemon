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

#include "thd_zone_generic.h"
#include "thd_engine.h"

cthd_zone_generic::cthd_zone_generic(int index, int _config_index,
		std::string type) :
		cthd_zone(index, ""), trip_point_cnt(0), config_index(_config_index), zone(
				NULL) {
	type_str = type;

}

int cthd_zone_generic::read_trip_points() {
	thermal_zone_t *zone_config = thd_engine->parser.get_zone_dev_index(
			config_index);
	int trip_point_cnt = 0;

	if (!zone_config)
		return THD_ERROR;
	for (unsigned int i = 0; i < zone_config->trip_pts.size(); ++i) {
		trip_point_t &trip_pt_config = zone_config->trip_pts[i];

		if (!trip_pt_config.temperature)
			continue;

		cthd_sensor *sensor = thd_engine->search_sensor(
				trip_pt_config.sensor_type);
		if (!sensor) {
			thd_log_error("XML zone: invalid sensor type \n");
			continue;
		}
		sensor_list.push_back(sensor);
		cthd_trip_point *trip_ptr = NULL;
		bool add = false;
		for (unsigned int j = 0; j < trip_points.size(); ++j) {
			if (trip_points[j].get_trip_type() == trip_pt_config.trip_pt_type) {
				thd_log_debug("updating existing trip temp \n");
				trip_points[j].update_trip_temp(trip_pt_config.temperature);
				trip_points[j].update_trip_hyst(trip_pt_config.hyst);
				trip_ptr = &trip_points[j];
				break;
			}
		}
		if (!trip_ptr) {
			trip_ptr = new cthd_trip_point(trip_point_cnt,
					trip_pt_config.trip_pt_type, trip_pt_config.temperature,
					trip_pt_config.hyst, index, sensor->get_index(),
					trip_pt_config.control_type);
			if (!trip_ptr) {
				thd_log_warn("Mem alloc error for new trip \n");
				return THD_ERROR;
			}
			if (trip_pt_config.trip_pt_type == MAX) {
				thd_model.set_max_temperature(trip_pt_config.temperature);
				if (thd_model.get_set_point()) {
					trip_ptr->update_trip_temp(thd_model.get_set_point());
				}
			}

			add = true;
		}
		// bind cdev
		for (unsigned int j = 0; j < trip_pt_config.cdev_trips.size(); ++j) {
			cthd_cdev *cdev = thd_engine->search_cdev(
					trip_pt_config.cdev_trips[j].type);
			if (cdev) {
				trip_ptr->thd_trip_point_add_cdev(*cdev,
						trip_pt_config.cdev_trips[j].influence,
						trip_pt_config.cdev_trips[j].sampling_period);
				zone_cdev_set_binded();
			}
		}
		if (add) {
			trip_points.push_back(*trip_ptr);
			++trip_point_cnt;
		}
		delete trip_ptr;
	}

	if (!trip_points.size()) {
		thd_log_info(
				" cthd_zone_generic::read_trip_points fail: No valid trips\n");
		return THD_ERROR;
	}

	return 0;
}

int cthd_zone_generic::read_cdev_trip_points() {
	return 0;
}

int cthd_zone_generic::zone_bind_sensors() {
	cthd_sensor *sensor;

	thermal_zone_t *zone_config = thd_engine->parser.get_zone_dev_index(
			config_index);

	if (!zone_config)
		return THD_ERROR;
	sensor = NULL;
	for (unsigned int i = 0; i < zone_config->trip_pts.size(); ++i) {
		trip_point_t &trip_pt_config = zone_config->trip_pts[i];
		sensor = thd_engine->search_sensor(trip_pt_config.sensor_type);
		if (!sensor) {
			thd_log_error("XML zone: invalid sensor type %s\n",
					trip_pt_config.sensor_type.c_str());
			continue;
		}
		bind_sensor(sensor);
	}
	if (!sensor)
		return THD_ERROR;

	return THD_SUCCESS;
}
