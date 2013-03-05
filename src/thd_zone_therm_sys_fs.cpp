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

void cthd_sysfs_zone::set_temp_sensor_path()
{
	std::stringstream ss;

	if(using_custom_zone)
	{
		temperature_sysfs_path = thd_engine->parser.get_sensor_path(index);
	}
	else
	{
		temperature_sysfs_path.assign("/sys/class/thermal/thermal_zone");
		ss << index << "/temp";
		temperature_sysfs_path.append(ss.str());
		thd_log_debug("set_sensor_path %s\n", temperature_sysfs_path.c_str());
	}
}

int cthd_sysfs_zone::read_trip_points()
{

	if(read_xml_trip_points() == THD_SUCCESS)
	{
		return THD_SUCCESS;
	}

	// Gather all trip points
	std::stringstream trip_sysfs;
	trip_sysfs << index << "/" << "trip_point_";
	for(int i = 0; i < max_trip_points; ++i)
	{
		std::stringstream type_stream;
		std::stringstream temp_stream;
		std::stringstream hist_stream;
		std::string type_str;
		std::string temp_str;
		std::string hyst_str;
		trip_point_type_t trip_type;
		unsigned int temp = 0, hyst = 1;

		type_stream << trip_sysfs.str() << i << "_type";
		if(zone_sysfs.exists(type_stream.str()))
		{
			zone_sysfs.read(type_stream.str(), type_str);
			thd_log_debug("Exists\n");
		}
		temp_stream << trip_sysfs.str() << i << "_temp";
		if(zone_sysfs.exists(temp_stream.str()))
		{
			zone_sysfs.read(temp_stream.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			thd_log_debug("Exists\n");
		}

		hist_stream << trip_sysfs.str() << i << "_hyst";
		if(zone_sysfs.exists(hist_stream.str()))
		{
			zone_sysfs.read(hist_stream.str(), hyst_str);
			std::istringstream(hyst_str) >> hyst;
			thd_log_debug("Exists\n");
		}

		if(type_str == "critical")
			trip_type = CRITICAL;
		else if(type_str == "hot")
			trip_type = HOT;
		else if(type_str == "active")
			trip_type = ACTIVE;
		else if(type_str == "passive")
			trip_type = PASSIVE;
		else
			trip_type = INVALID_TRIP_TYPE;
		if(trip_type != INVALID_TRIP_TYPE)
		{
			cthd_trip_point trip_pt(trip_point_cnt, trip_type, temp, hyst, 0);
			trip_points.push_back(trip_pt);
			++trip_point_cnt;
		}
	}
	if(trip_point_cnt == 0)
		return THD_ERROR;
	else
		return THD_SUCCESS;
}

int cthd_sysfs_zone::read_cdev_trip_points()
{
	thd_log_debug(" >> read_cdev_trip_points for \n");
	if(read_xml_cdev_trip_points() == THD_SUCCESS)
		return THD_SUCCESS;

	// Gather all Cdevs
	// Gather all trip points
	std::stringstream cdev_sysfs;
	cdev_sysfs << index << "/" << "cdev";
	for(int i = 0; i < max_cool_devs; ++i)
	{
		std::stringstream trip_pt_stream, cdev_stream;
		std::string trip_pt_str;
		int trip_cnt =  - 1;
		char buf[50],  *ptr;
		trip_pt_stream << cdev_sysfs.str() << i << "_trip_point";

		thd_log_debug("check for %s \n", trip_pt_stream.str().c_str());

		if(zone_sysfs.exists(trip_pt_stream.str()))
		{
			zone_sysfs.read(trip_pt_stream.str(), trip_pt_str);
			std::istringstream(trip_pt_str) >> trip_cnt;
		}
		else
			continue;
		thd_log_debug("cdev trip point: %s contains %d\n", trip_pt_str.c_str(),
	trip_cnt);
		cdev_stream << cdev_sysfs.str() << i;
		if(zone_sysfs.exists(cdev_stream.str()))
		{
			thd_log_debug("cdev%d present\n", i);
			int ret = zone_sysfs.read_symbolic_link_value(cdev_stream.str(), buf, 50);
			if(ret == 0)
			{
				ptr = strstr(buf, "cooling_device");
				if(ptr)
				{
					ptr += strlen("cooling_device");
					thd_log_debug("symbolic name %s:%s\n", buf, ptr);
					if(trip_cnt >= 0)
					{
						trip_points[trip_cnt].thd_trip_point_add_cdev_index(atoi(ptr));
					}
				}
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_sysfs_zone::read_xml_cdev_trip_points()
{
	if(using_custom_zone)
		return THD_SUCCESS;
	else
		return THD_ERROR;
}

int cthd_sysfs_zone::read_xml_trip_points()
{
	trip_point_cnt = 0;
	thd_log_debug("read_xml_trip_points: %d \n", thd_engine->use_custom_zones());
	if(thd_engine->use_custom_zones())
	{
		thd_log_debug("Use Custom zone info for zone %d \n", index);
		for(int i = 0; i < thd_engine->parser.trip_count(index); ++i)
		{
			trip_point_t *trip_pt;
			int trip_type;

			trip_pt = thd_engine->parser.get_trip_point(index, i);
			if(!trip_pt)
				continue;

			cthd_trip_point trip_pt_obj(trip_point_cnt, trip_pt->trip_pt_type, trip_pt
	->temperature, trip_pt->hyst, 0);
			trip_pt_obj.thd_trip_point_add_cdev_index(trip_pt->cool_dev_id);
			trip_points.push_back(trip_pt_obj);
			++trip_point_cnt;
		}
		if(trip_point_cnt)
		{
			return THD_SUCCESS;
			using_custom_zone = true;
		}
	}
	return THD_ERROR;
}
