/*
 * thd_trip_point.h: thermal zone trip points class interface
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

#ifndef THD_TRIP_POINT_H
#define THD_TRIP_POINT_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_cdev.h"
#include "thd_pid.h"

#include <vector>

typedef enum {
	CRITICAL,
	HOT,
	PASSIVE,
	ACTIVE,
	INVALID_TRIP_TYPE
}trip_point_type_t;
	
class cthd_trip_point {
private:
	int					index;
	trip_point_type_t	type;
	unsigned int 		temp;
	unsigned int 		hyst;
	std::vector <int> 	cdev_indexes;
	cthd_pid			pid_controller;
	bool				pid_enable;

public:
	cthd_trip_point(int _index, trip_point_type_t _type, unsigned int _temp, unsigned int _hyst);
	bool thd_trip_point_check(unsigned int read_temp, int pref);
	void thd_trip_point_add_cdev_index(int index);
	void thd_set_pid(cthd_pid &pid_ctrl);
};

#endif
