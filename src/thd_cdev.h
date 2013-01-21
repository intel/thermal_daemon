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

#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"

class cthd_cdev {

protected:
	int				index;

	unsigned int 	curr_state;
	unsigned int 	max_state;
	unsigned int 	min_state;
	csys_fs 		cdev_sysfs;
	unsigned int 	trip_point;
	std::string 	type_str;

private:

public:
	cthd_cdev(unsigned int _index, std::string control_path) :
		index(_index),
		cdev_sysfs(control_path.c_str()),
		trip_point(0),
		max_state(0),
		min_state(0),
		curr_state(0)
	{}

	virtual void thd_cdev_set_state(int set_point, int temperature, int state){
		thd_log_debug(">>thd_cdev_set_state index:%d state:%d\n", index, state);
		curr_state = get_curr_state();
		max_state = get_max_state();
		thd_log_debug("thd_cdev_set_%d:curr state %d max state %d\n", index, curr_state, max_state);
		if (state) {
			if (curr_state < max_state) {
				thd_log_debug("device:%s %d\n", type_str.c_str(), curr_state+1);
				set_curr_state(curr_state + 1);
			}
		} else {
			if (curr_state > 0) {
				thd_log_info("device:%s %d\n", type_str.c_str(), curr_state);
				set_curr_state(curr_state - 1);
			}
		}
		thd_log_info("Set : %d, %d, %d, %d, %d\n", set_point, temperature, index, get_curr_state(), max_state);

		thd_log_debug("<<thd_cdev_set_state %d\n", state);
	};

	int thd_cdev_get_index() { return index; }

	virtual int init() {return 0;};
	virtual int control_begin() {return 0;};
	virtual int control_end() {return 0;};
	virtual void set_curr_state(int state) {}

	virtual int get_curr_state()
	{
		thd_log_debug("cthd_cdev_pstates::get_curr_state() index %d state %d\n", index, curr_state);
		return curr_state;
	}

	virtual int get_max_state() { return 0;};
	virtual int update() {return 0;} ;
};

#endif
