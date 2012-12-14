/*
 * thd_cdev.h: thermal cooling class interface
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

#ifndef THD_CDEV_H
#define THD_CDEV_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"

class cthd_cdev {
private:
	int			index;
	csys_fs 		cdev_sysfs;
	unsigned int 		trip_point;
	unsigned int 		curr_state;
	unsigned int 		max_state;
	std::string 		type_str;

public:
	cthd_cdev(unsigned int _index);
	void thd_cdev_set_state(int state);
	int thd_cdev_get_index() { return index; }

	virtual void set_curr_state(int state);
	virtual int get_curr_state();
	virtual int get_max_state();
	virtual int update();
};

#endif
