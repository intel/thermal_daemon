/*
 * thd_zone_dts.cpp: thermal engine DTS class implementation
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

#include "thd_zone_dts.h"
#include "thd_engine.h"
#include "thd_msr.h"

cthd_zone_dts::cthd_zone_dts(int index, std::string path)
	: cthd_zone(index, path),
	  sensor_mask(0),
	  dts_sysfs(path.c_str()),
	  trip_point_cnt(0)
{

}

int	cthd_zone_dts::init()
{
	critical_temp = 0;
	int temp;

	for (int i=0; i<max_dts_sensors; ++i) {
		std::stringstream temp_crit_str;
//		temp_crit_str << "temp" << i << "_crit";
		temp_crit_str << "temp" << i << "_max";

		if (dts_sysfs.exists(temp_crit_str.str())) {

			// Set which index is present
			sensor_mask = sensor_mask | (1<<i);

			std::string temp_str;
			dts_sysfs.read(temp_crit_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if (critical_temp == 0 || temp < critical_temp)
				critical_temp = temp;
		}
	}
	thd_log_debug("Core temp DTS :critical %d\n", critical_temp);

	thd_model.set_max_temperature(critical_temp);
	prev_set_point = set_point = thd_model.get_set_point();

	return THD_SUCCESS;
}

int cthd_zone_dts::read_trip_points()
{
	init();

	cthd_trip_point trip_pt(trip_point_cnt, 0, set_point, def_hystersis);
	trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
	trip_pt.thd_trip_point_add_cdev_index(0);
	trip_pt.thd_trip_point_add_cdev_index(1);
	trip_pt.thd_trip_point_add_cdev_index(2);
	trip_points.push_back(trip_pt);
	trip_point_cnt++;

	return THD_SUCCESS;
}

int cthd_zone_dts::update_trip_points()
{
	if (prev_set_point == set_point)
		return THD_SUCCESS;

	for (int i=0; i<trip_point_cnt; ++i) {
		cthd_trip_point& trip_pt = trip_points[i];
		if (trip_pt.thd_trip_point_value() == prev_set_point) {
			trip_pt.thd_trip_update_set_point(set_point);
			break;
		}
	}

	return THD_SUCCESS;
}

int cthd_zone_dts::read_cdev_trip_points()
{
	return THD_SUCCESS;

}

void cthd_zone_dts::set_temp_sensor_path()
{

}

unsigned int cthd_zone_dts::read_zone_temp()
{
	unsigned int mask = 0x1;
	unsigned int temp;
	int cnt = 0;

	zone_temp = 0;
	do {
		if (sensor_mask & mask) {
			std::stringstream temp__input_str;
			temp__input_str << "temp" << cnt << "_input";
			if (dts_sysfs.exists(temp__input_str.str())) {
				std::string temp_str;
				dts_sysfs.read(temp__input_str.str(), temp_str);
				std::istringstream(temp_str) >> temp;
				if (zone_temp == 0 || temp > zone_temp)
					zone_temp = temp;
				thd_log_debug("dts %d: temp %u zone_temp %u\n", cnt, temp, zone_temp);
			}
		}
		mask = (mask << 1);
		cnt++;
	} while(mask != 0);

	thd_model.add_sample(zone_temp);
	if (thd_model.is_set_point_reached()) {
		prev_set_point = set_point;
		set_point = thd_model.get_set_point();
		thd_log_debug("new set point %d \n", set_point);
		update_trip_points();
	}
#if 0
{
	cthd_msr msr;
	thd_log_debug("perf_status %X\n", msr.read_perf_status());
}
#endif

	return zone_temp;
}
