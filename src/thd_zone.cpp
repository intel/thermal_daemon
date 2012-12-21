/*
 * thd_zone.cpp: thermal zone class implentation
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
 */

#include "thd_zone.h"

cthd_zone::cthd_zone(int _index)
	: index(_index),
	  zone_sysfs("/sys/class/thermal/thermal_zone"),
	  trip_point_cnt(0),
	  zone_temp(0),
	  cool_dev_cnt(0),
	  cdev_mask(0),
	  temperature_sysfs_path("")
{
	thd_log_debug("Added zone index:%d \n", index);
}

int cthd_zone::read_trip_points()
{

	// Gather all trip points
	std::stringstream trip_sysfs;
	trip_sysfs << index << "/" << "trip_point_";
	for (int i=0; i<max_trip_points; ++i) {
		std::stringstream type_stream;
		std::stringstream temp_stream;
		std::stringstream hist_stream;
		std::string type_str;
		std::string temp_str;
		std::string hyst_str;
		trip_point_type_t trip_type;
		unsigned int temp=0, hyst=1;

		type_stream << trip_sysfs.str() << i << "_type";
		if (zone_sysfs.exists(type_stream.str())) {
			zone_sysfs.read(type_stream.str(), type_str);
			thd_log_debug("Exists\n");
		}
		temp_stream << trip_sysfs.str() << i << "_temp";
		if (zone_sysfs.exists(temp_stream.str())) {
			zone_sysfs.read(temp_stream.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			thd_log_debug("Exists\n");
		}

		hist_stream << trip_sysfs.str() << i << "_hyst";
		if (zone_sysfs.exists(hist_stream.str())) {
			zone_sysfs.read(hist_stream.str(), hyst_str);
			std::istringstream(hyst_str) >> hyst;
			thd_log_debug("Exists\n");
		}

		if (type_str == "critical")
			trip_type = CRITICAL;
		else if (type_str == "hot")
			trip_type = HOT;
		else if (type_str == "active")
			trip_type = ACTIVE;
		else if (type_str == "passive")
			trip_type = PASSIVE;
		else
			trip_type = INVALID_TRIP_TYPE;
		if (trip_type != INVALID_TRIP_TYPE) {
			cthd_trip_point trip_pt(trip_point_cnt, trip_type, temp, hyst);
			trip_points.push_back(trip_pt);
			++trip_point_cnt;
		}
	}
	if (trip_point_cnt == 0)
		return THD_ERROR;
	else
		return THD_SUCCESS;
}

int cthd_zone::read_cdev_trip_points()
{
	// Gather all Cdevs
	// Gather all trip points
	std::stringstream cdev_sysfs;
	cdev_sysfs << index << "/" << "cdev";
	for (int i=0; i<max_cool_devs; ++i) {
		std::stringstream trip_pt_stream, cdev_stream;
		std::string trip_pt_str;
		int trip_cnt = -1;
		char buf[50], *ptr;
		trip_pt_stream << cdev_sysfs.str() << i << "_trip_point";
		if (zone_sysfs.exists(trip_pt_stream.str())) {
			zone_sysfs.read(trip_pt_stream.str(), trip_pt_str);
			std::istringstream(trip_pt_str) >> trip_cnt;
		} else
			continue;
		thd_log_debug("cdev trip point: %s contains %d\n", trip_pt_str.c_str(), trip_cnt);
		cdev_stream << cdev_sysfs.str() << i;
		if (zone_sysfs.exists(cdev_stream.str())) {
			thd_log_debug("cdev%d present\n", i);
			int ret = zone_sysfs.read_symbolic_link_value(cdev_stream.str(), buf, 50);
			if (ret == 0) {
				ptr = strstr(buf, "cooling_device");
				if (ptr) {
					ptr += strlen("cooling_device");
					thd_log_debug("symbolic name %s:%s\n", buf, ptr);
					if (trip_cnt >= 0) {
						trip_points[trip_cnt].thd_trip_point_add_cdev_index(atoi(ptr));
						pid_control.add_cdev_index(atoi(ptr));
					}
				}
			}
		}
	}

	return THD_SUCCESS;
}


void cthd_zone::thermal_zone_change()
{
	int i, count;
	cthd_preference thd_pref;

	count = trip_points.size();
	for (i=count -1; i >= 0; --i)	{
		cthd_trip_point trip_point = trip_points[i];
		trip_point.thd_trip_point_check(zone_temp, thd_pref.get_preference());
	}
}
void cthd_zone::update_zone_preference()
{

	int i, count;
	cthd_preference thd_pref;

	thd_log_debug("update_zone_preference\n");
	count = trip_points.size();
	for (i=count -1; i >= 0; --i)	{
		cthd_trip_point trip_point = trip_points[i];
		trip_point.thd_trip_point_check(0, thd_pref.get_old_preference());
	}

	for (i=count -1; i >= 0; --i)	{
		cthd_trip_point trip_point = trip_points[i];
		trip_point.thd_trip_point_check(read_zone_temp(), thd_pref.get_preference());
	}

}

int cthd_zone::thd_update_zones()
{
	int ret;

	set_sensor_path();

	enable_disable_pid_controller();
	if (enable_pid) {
		get_pid_params();
		thd_log_debug("pid controller address %x \n", &pid_control);
		pid_control.set_pid_params(Kp, Ki, Kd);
	}

	ret = read_trip_points();
	if (ret != THD_SUCCESS)
		return THD_ERROR;

	ret = read_cdev_trip_points();
	if (ret != THD_SUCCESS)
		return THD_ERROR;

	if (enable_pid) {
		pid_control.print_pid_constants();

	}
	return THD_SUCCESS;
}

void cthd_zone::zone_temperature_notification(int type, int data)
{
	read_zone_temp();
	thermal_zone_change();
}

void cthd_zone::set_sensor_path()
{
	std::stringstream ss;

	temperature_sysfs_path.assign("/sys/class/thermal/thermal_zone");
	ss << index << "/temp";
	temperature_sysfs_path.append(ss.str());
	thd_log_debug("set_sensor_path %s\n", temperature_sysfs_path.c_str());
}

unsigned int cthd_zone::read_zone_temp()
{
	csys_fs sysfs;
	std::string buffer;

	thd_log_debug("read_zone_temp \n");
	if (!sysfs.exists(temperature_sysfs_path)) {
		thd_log_warn("read_zone_temp: No temp sysfs for reading temp: %s\n", temperature_sysfs_path.c_str());
		return zone_temp;
	}
	sysfs.read(temperature_sysfs_path, buffer);
	std::istringstream(buffer) >> zone_temp;
	thd_log_info("Zone temp %u \n", zone_temp);

	return zone_temp;
}
