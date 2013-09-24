/*
 * thd_zone.cpp: thermal zone class implentation
 *
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

/* This class implements parent thermal zone/sensor. It is included
 * in a thermal engine. During initialization, it establishes a
 * relationship between cooling devices and trip points (where
 * some action needs to be taken).
 * When it gets a notification for a change, it reads the temperature
 * from sensors and uses cthd_trip point to schedule action on the event
 * if required.
 */

#include "thd_zone.h"


cthd_zone::cthd_zone(int _index, std::string control_path): index(_index),
	zone_sysfs(control_path.c_str()), zone_temp(0), zone_active(false)
{
	thd_log_debug("Added zone index:%d \n", index);
}

unsigned int cthd_zone::read_zone_temp()
{
	csys_fs sysfs;
	std::string buffer;

	thd_log_debug("read_zone_temp %s\n", temperature_sysfs_path.c_str());
	if(!sysfs.exists(temperature_sysfs_path))
	{
		thd_log_warn("read_zone_temp: No temp sysfs for reading temp: %s\n",
	temperature_sysfs_path.c_str());
		return zone_temp;
	}
	sysfs.read(temperature_sysfs_path, buffer);
	std::istringstream(buffer) >> zone_temp;
	thd_log_debug("Zone temp %u \n", zone_temp);

	return zone_temp;
}

void cthd_zone::thermal_zone_temp_change()
{
	int i, count;
	cthd_preference thd_pref;

	count = trip_points.size();
	for(i = count - 1; i >= 0; --i)
	{
		cthd_trip_point &trip_point = trip_points[i];
		trip_point.thd_trip_point_check(zone_temp, thd_pref.get_preference());
	}
}

void cthd_zone::update_zone_preference()
{

	int i, count;
	cthd_preference thd_pref;

	thd_log_debug("update_zone_preference\n");
	count = trip_points.size();
	for(i = count - 1; i >= 0; --i)
	{
		cthd_trip_point trip_point = trip_points[i];
		trip_point.thd_trip_point_check(0, thd_pref.get_old_preference());
	}

	for(i = count - 1; i >= 0; --i)
	{
		cthd_trip_point trip_point = trip_points[i];
		trip_point.thd_trip_point_check(read_zone_temp(), thd_pref.get_preference());
	}
}

int cthd_zone::zone_update()
{
	int ret;

	ret = read_trip_points();
	if(ret != THD_SUCCESS)
		return THD_ERROR;

	ret = read_cdev_trip_points();
	if(ret != THD_SUCCESS)
		return THD_ERROR;

	set_temp_sensor_path();

	return THD_SUCCESS;
}

void cthd_zone::zone_temperature_notification(int type, int data)
{
	if(zone_active)
	{
		read_zone_temp();
		thermal_zone_temp_change();
	}
}

void cthd_zone::zone_reset()
{
	int i, count;

	if(zone_active)
	{
		count = trip_points.size();
		for(i = count - 1; i >= 0; --i)
		{
			cthd_trip_point &trip_point = trip_points[i];
			trip_point.thd_trip_cdev_state_reset();
		}
	}
}
