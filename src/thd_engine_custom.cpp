/*
 * thd_engine_custom.cpp: thermal engine custom class implementation
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
 * This interface allows to overide reading of zones and cooling devices from
 * default ACPI sysfs. 
 */

#include "thd_engine_custom.h"

int cthd_engine_custom::read_thermal_zones()
{
	int count = 0;

	parse_success = false;

	try{

		if (parser.parser_init() != THD_SUCCESS)
			throw THD_ERROR;
		if (parser.start_parse() != THD_SUCCESS)
			throw THD_ERROR;

		parser.dump_thermal_conf();

		if (parser.platform_matched()) {
			parse_success = true;
			thd_log_info("uuid matched zone-cnt %d\n", parser.zone_count());
			for (int i=0; i<parser.zone_count(); ++i) {
				cthd_zone_custom zone(i);
				if (zone.thd_update_zones() != THD_SUCCESS)
					continue;
				zones.push_back(zone);
				++count;
			}
		} else
			return cthd_engine::read_thermal_zones();

		parser.parser_deinit();
	}
	catch(int e)
	{
		return cthd_engine::read_thermal_zones();;
	}

	return THD_SUCCESS;
}

int cthd_engine_custom::read_cooling_devices()
{

	if (parser.platform_matched()) {
		parse_success = true;
		cdev_cnt = 0;
		thd_log_info("uuid matched cdev-cnt %d\n", parser.cdev_count());
		for (int i=0; i<parser.cdev_count(); ++i) {
			cthd_cdev_custom cdev(cdev_cnt);
			if (cdev.update() != THD_SUCCESS)
				continue;
			cdevs.push_back(cdev);
			++cdev_cnt;
		}
		return THD_SUCCESS;
	} else
		return cthd_engine::read_cooling_devices();
}

