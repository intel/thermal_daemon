/*
 * thd_zone.h: thermal zone class interface
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

#ifndef THD_ZONE_H
#define THD_ZONE_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_cdev.h"
#include "thd_trip_point.h"
#include <vector>

typedef struct {
	int zone;
	int type;
	unsigned int data;
}thermal_zone_notify_t;

class cthd_zone {
private:
	static const int		max_trip_points = 50;
	static const int		max_cool_devs = 50;
	int				index;
	std::vector <cthd_trip_point> 	trip_points;
	int				trip_point_cnt;
	std::vector <cthd_cdev> 	assoc_cdevs;
	int				cool_dev_cnt;
	unsigned long			cdev_mask;
	csys_fs 			zone_sysfs;
	unsigned int 			zone_temp;
	unsigned int			read_zone_temp();
	void				thermal_zone_change();

public:
	cthd_zone(int index);
	void zone_temperature_notification(int type, int data);
	virtual int read_trip_points();
	virtual int read_cdev_trip_points();
	int thd_update_zones();
};

#endif
