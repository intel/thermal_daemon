/*
 * thd_cdev_custom.cpp: thermal engine custom cdev class implementation
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
 * This implementation allows to overide per cdev read data from sysfs for buggy BIOS. 
 */

#include "thd_cdev_custom.h"
#include "thd_engine.h"
#include "thd_engine_custom.h"

void cthd_cdev_custom::set_curr_state(int state)
{
	if (use_custom) {
		csys_fs sys_fs("");
		if (sys_fs.exists(path_str)) {
			std::stringstream state_str;
			state_str << state;
			sys_fs.write(path_str, state_str.str());
		}
		thd_log_warn("cthd_cdev_custom: set_curr_state failed\n");
	}
	else
		return cthd_cdev::set_curr_state(state);
}

int cthd_cdev_custom::get_curr_state()
{
	if (use_custom) {
		csys_fs sys_fs("");
		if (sys_fs.exists(path_str)) {
				std::string state_str;
				sys_fs.read(path_str, state_str);
				std::istringstream(state_str) >> curr_state;
				return curr_state;
		}
		thd_log_warn("cthd_cdev_custom: get_curr_state failed\n");
		return 0;
	}
	else
		return cthd_cdev::get_curr_state();
}

int cthd_cdev_custom::get_max_state()
{
	if (use_custom)
		return max_state;
	else
		return cthd_cdev::get_max_state();
}

int cthd_cdev_custom::update()
{
	use_custom = 0;

	if (thd_engine.use_custom_engine()){
		cooling_dev_t *cdev;
		csys_fs sys_fs("");

		cdev = thd_engine.parser.get_cool_dev_index(index);
		if (cdev) {
			use_custom = 1;
			path_str = cdev->path_str;
			min_state = cdev->min_state;
			max_state = cdev->max_state;
			if (sys_fs.exists(path_str)) {
				std::string state_str;
				sys_fs.read(path_str, state_str);
				std::istringstream(state_str) >> curr_state;
			} else
				curr_state = 0;

			return THD_SUCCESS;
		} else
			return cthd_cdev::update();
	} else
		return cthd_cdev::update();
}
