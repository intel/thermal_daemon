/*
 * thd_zone_therm_sys_fs.h: thermal zone class interface
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

#ifndef THD_ZONE_THERM_SYS_FS_H_
#define THD_ZONE_THERM_SYS_FS_H_

#include "thd_zone.h"

class cthd_sysfs_zone: public cthd_zone {
private:
	int trip_point_cnt;
	cthd_zone *zone;

public:
	static const int max_trip_points = 50;
	static const int max_cool_devs = 50;

	cthd_sysfs_zone(int count, std::string path);

	virtual int read_trip_points();
	virtual int read_cdev_trip_points();
	virtual int zone_bind_sensors();

};

#endif /* THD_ZONE_THERM_SYS_FS_H_ */
