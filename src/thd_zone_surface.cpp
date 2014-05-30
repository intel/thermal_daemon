/*
 * thd_zone_surface.cpp: zone implementation for external surface
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

#include "thd_zone_surface.h"

const char *skin_sensors[] = { "TSKN", "SKIN", "skin", "tskin", "TSKIN",
		"cover", NULL };

cthd_zone_surface::cthd_zone_surface(int index) :
		cthd_zone(index, ""), sensor(NULL) {
	type_str = "Surface";
}

int cthd_zone_surface::read_trip_points() {
	cthd_trip_point *trip_ptr = NULL;
	bool add = false;
	int i = 0;
	cthd_zone *ref_zone = NULL;

	if (!sensor)
		return THD_ERROR;

	while (skin_sensors[i]) {
		cthd_zone *zone;

		zone = thd_engine->search_zone(skin_sensors[i]);
		if (zone) {
			ref_zone = zone;
			if (zone->zone_active_status()) {
				thd_log_error("A skin sensor zone is already active \n");
				return THD_ERROR;
			}
			break;
		}
		++i;
	}

	for (unsigned int j = 0; j < trip_points.size(); ++j) {
		if (trip_points[j].get_trip_type() == PASSIVE) {
			thd_log_debug("updating existing trip temp \n");
			trip_points[j].update_trip_temp(passive_trip_temp);
			trip_points[j].update_trip_hyst(passive_trip_hyst);
			trip_ptr = &trip_points[j];
			break;
		}
	}
	if (!trip_ptr) {
		trip_ptr = new cthd_trip_point(trip_points.size(), MAX,
				passive_trip_temp, passive_trip_hyst, index,
				sensor->get_index(), SEQUENTIAL);
		if (!trip_ptr) {
			thd_log_warn("Mem alloc error for new trip \n");
			return THD_ERROR;
		}
		add = true;
	}

	cthd_cdev *cdev = thd_engine->search_cdev("rapl_controller");
	if (cdev) {
		trip_ptr->thd_trip_point_add_cdev(*cdev,
				cthd_trip_point::default_influence, surface_sampling_period);
	}
	cdev = thd_engine->search_cdev("intel_powerclamp");
	if (cdev) {
		trip_ptr->thd_trip_point_add_cdev(*cdev,
				cthd_trip_point::default_influence, surface_sampling_period);
	}

	if (add) {
		trip_points.push_back(*trip_ptr);
	}
	delete trip_ptr;

	if (ref_zone)
		ref_zone->zone_cdev_set_binded();

	return THD_SUCCESS;
}

int cthd_zone_surface::zone_bind_sensors() {
	int i = 0;

	while (skin_sensors[i]) {
		sensor = thd_engine->search_sensor(skin_sensors[i]);
		if (sensor)
			break;
		++i;
	}
	if (!sensor) {
		thd_log_error("No SKIN sensor \n");
		return THD_ERROR;
	}
	bind_sensor(sensor);

	return THD_SUCCESS;
}

int cthd_zone_surface::read_cdev_trip_points() {
	return THD_SUCCESS;
}
