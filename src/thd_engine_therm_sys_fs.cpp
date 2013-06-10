/*
 * thd_engine_therm_sys_fs.h: thermal engine class implementation
 *  for acpi style	sysfs control.
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

#include "thd_zone_therm_sys_fs.h"
#include "thd_engine_therm_sys_fs.h"
#include "thd_cdev_therm_sys_fs.h"

/* Implements ACPI style thermal zone. If there is a UUID match, this will
 * load each trip ponts and cooling device ids from XML.
 *
 */

cthd_engine_therm_sysfs::cthd_engine_therm_sysfs(): thd_sysfs(
	"/sys/class/thermal/"), parser_init_done(false)
{
	parser_init();
}

int cthd_engine_therm_sysfs::read_thermal_zones()
{
	int count = zone_count;

	// Check if the configuration is defined in the configuration file
	if(read_xml_thermal_zones() == THD_SUCCESS)
	{
		thd_log_info("Thermal zones will be loaded from config file\n");
		set_control_mode(EXCLUSIVE); // This will manage the thermal for the system
		return THD_SUCCESS;
	}

	// Check for the presence of thermal zones
	if(thd_sysfs.exists("thermal_zone"))
	{
		cthd_sysfs_zone *zone = new cthd_sysfs_zone(count, 
	"/sys/class/thermal/thermal_zone");
		zone->set_zone_active();
		zones.push_back(zone);
		++count;
	}
	for(int i = 0; i < max_thermal_zones; ++i)
	{
		std::stringstream tzone;
		tzone << "thermal_zone" << i;
		if(thd_sysfs.exists(tzone.str().c_str()))
		{
			cthd_sysfs_zone *zone = new cthd_sysfs_zone(count, 
	"/sys/class/thermal/thermal_zone");
			zone->set_zone_active();
			if(zone->zone_update() != THD_SUCCESS)
				continue;
			zones.push_back(zone);
			++count;
		}
	}
	thd_log_debug("%d thermal zones present\n", count);

	if(!count)
	{
		thd_log_error("Thermal sysfs: No Zones present: \n");
		return THD_FATAL_ERROR;
	}

	set_control_mode(EXCLUSIVE); // This will manage the thermal for the system

	return THD_SUCCESS;
}

int cthd_engine_therm_sysfs::read_cooling_devices()
{
	if(read_xml_cooling_device() == THD_SUCCESS)
	{
		thd_log_info("Thermal cooling devices will be loaded from config file\n");
		return THD_SUCCESS;
	}

	for(int i = 0; i < max_cool_devs; ++i)
	{
		std::stringstream tcdev;
		tcdev << "cooling_device" << i;
		if(thd_sysfs.exists(tcdev.str().c_str()))
		{
			cthd_sysfs_cdev *cdev = new cthd_sysfs_cdev(cdev_cnt, "/sys/class/thermal/");
			if(cdev->update() != THD_SUCCESS)
				continue;
			cdevs.push_back(cdev);
			++cdev_cnt;
		}
	}
	thd_log_debug("%d cooling device present\n", cdev_cnt);

	if(!cdev_cnt)
	{
		thd_log_error("Thermal sysfs: No cooling devices: \n");
		return THD_FATAL_ERROR;
	}

	return THD_SUCCESS;
}

void cthd_engine_therm_sysfs::parser_init()
{
	if(parser.parser_init() == THD_SUCCESS)
	{
		if(parser.start_parse() == THD_SUCCESS)
		{
			parser.dump_thermal_conf();
			parser_init_done = true;
		}
	}
}

int cthd_engine_therm_sysfs::read_xml_thermal_zones()
{
	int count = zone_count;

	parse_thermal_zone_success = false;
	if(parser_init_done && parser.platform_matched() && parser.zone_count())
	{
		thd_log_info("uuid matched zone-cnt %d\n", parser.zone_count());
		parse_thermal_zone_success = true;

		for(int i = 0; i < parser.zone_count(); ++i)
		{
			cthd_sysfs_zone *zone = new cthd_sysfs_zone(count, 
	"/sys/class/thermal/thermal_zone/");
			if(zone->zone_update() != THD_SUCCESS)
			{
				return THD_ERROR;
			}
			parser.set_default_preference();
			zone->set_zone_active();
			zones.push_back(zone);
			++count;
		}
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_engine_therm_sysfs::read_xml_cooling_device()
{
	if(parser_init_done && parser.platform_matched() && parser.cdev_count())
	{
		thd_log_info("uuid matched cdev-cnt %d\n", parser.cdev_count());
		parse_thermal_cdev_success = true;
		for(int i = 0; i < parser.cdev_count(); ++i)
		{
			cthd_sysfs_cdev *cdev = new cthd_sysfs_cdev(cdev_cnt, "/sys/class/thermal/");
			if(cdev->update() != THD_SUCCESS)
				continue;
			cdevs.push_back(cdev);
			++cdev_cnt;
		}
		return THD_SUCCESS;
	}

	return THD_ERROR;
}
