/*
 * thd_zone.h: thermal zone class interface
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

#ifndef THD_ZONE_H
#define THD_ZONE_H

#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_cdev.h"
#include "thd_trip_point.h"

#include <vector>

typedef struct
{
	int zone;
	int type;
	unsigned int data;
} thermal_zone_notify_t;

class cthd_zone
{

protected:
	int index;
	std::vector < cthd_trip_point > trip_points;
	std::string temperature_sysfs_path;
	csys_fs zone_sysfs;
	unsigned int zone_temp;
	bool zone_active;

private:
	void thermal_zone_temp_change();

public:
	cthd_zone(int _index, std::string control_path);
	virtual ~cthd_zone(){}
	void zone_temperature_notification(int type, int data);
	int zone_update();
	virtual void update_zone_preference();
	;

	virtual unsigned int read_zone_temp();
	virtual int read_trip_points() = 0;
	virtual int read_cdev_trip_points() = 0;
	virtual void set_temp_sensor_path() = 0;

	void set_zone_active()
	{
		zone_active = true;
	};
	bool zone_active_status()
	{
		return zone_active;
	};
};

#endif
