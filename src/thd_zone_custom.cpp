/*
 * thd_zone_custom.h: thermal engine custom class implementation
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
 * This implementation allows to overide per zone read data from
 * sysfs for buggy BIOS. 
 */

#include "thd_zone_custom.h"
#include "thd_engine.h"
#include "thd_engine_custom.h"

int cthd_zone_custom::read_trip_points()
{
	trip_point_cnt = 0;
	if (thd_engine.use_custom_engine()){
		thd_log_debug("Use Custom zone info for zone %d \n", index);
		for (int i=0; i<thd_engine.parser.trip_count(index); ++i) {
			trip_point_t *trip_pt;
			int trip_type;

			trip_pt = thd_engine.parser.get_trip_point(index, i);
			if (!trip_pt)
				continue;

			cthd_trip_point trip_pt_obj(trip_point_cnt, trip_pt->trip_pt_type, trip_pt->temperature, trip_pt->hyst);
			trip_pt_obj.thd_trip_point_add_cdev_index(trip_pt->cool_dev_id);
			if (enable_pid) {
				pid_control.add_cdev_index(trip_pt->cool_dev_id);
				trip_pt_obj.thd_set_pid(pid_control);
			}
			trip_points.push_back(trip_pt_obj);
			++trip_point_cnt;
		}
		return THD_SUCCESS;
	} else {
		thd_log_debug("Use parent method \n");
		return cthd_zone::read_trip_points();
	}
}

int cthd_zone_custom::read_cdev_trip_points()
{
	if (thd_engine.use_custom_engine()){
		return THD_SUCCESS;
	}
	return cthd_zone::read_cdev_trip_points();
}

void cthd_zone_custom::enable_disable_pid_controller()
{
	enable_pid = false;
	if (thd_engine.use_custom_engine()){
		enable_pid = thd_engine.parser.pid_status(index);
	} else
		cthd_zone::enable_disable_pid_controller();
}

void cthd_zone_custom::get_pid_params()
{
	if (thd_engine.use_custom_engine()){
		enable_pid = thd_engine.parser.pid_status(index);
		thd_engine.parser.get_pid_values(index, &Kp, &Ki, &Kd);
	} else
		cthd_zone::get_pid_params();
}

void cthd_zone_custom::set_sensor_path()
{
	if (thd_engine.use_custom_engine()){
		thd_log_debug("using custom sensor path\n");
		temperature_sysfs_path = thd_engine.parser.get_sensor_path(index);
	} else
		cthd_zone::set_sensor_path();
}
