/*
 * thd_cdev.cpp: thermal cooling class implementation
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
#include "thd_cdev.h"

cthd_cdev::cthd_cdev(unsigned int _index)
					: index(_index),
					  cdev_sysfs("/sys/class/thermal/")
{
}

int cthd_cdev::update()
{
	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/cur_state";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::string state_str;
		cdev_sysfs.read(tc_state_dev.str(), state_str);
		std::istringstream(state_str) >> curr_state;
	} else
		curr_state = 0;

	std::stringstream tc_max_state_dev;
	tc_max_state_dev << "cooling_device" << index << "/max_state";
	if (cdev_sysfs.exists(tc_max_state_dev.str())) {
		std::string state_str;
		cdev_sysfs.read(tc_max_state_dev.str(), state_str);
		std::istringstream(state_str) >> max_state;
	} else
		max_state = 0;

	std::stringstream tc_type_dev;
	tc_type_dev << "cooling_device" << index << "/type";
	if (cdev_sysfs.exists(tc_type_dev.str())) {
		cdev_sysfs.read(tc_type_dev.str(), type_str);
	}

	thd_log_debug("cooling dev %d:%d:%d:%s\n", index, curr_state, max_state, type_str.c_str());

	return THD_SUCCESS;
}

int cthd_cdev::get_max_state()
{
	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/max_state";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::string state_str;
		cdev_sysfs.read(tc_state_dev.str(), state_str);
		std::istringstream(state_str) >> max_state;
	} else
		max_state = 0;

	return max_state;
}

void cthd_cdev::set_curr_state(int state)
{

	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/cur_state";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::stringstream state_str;
		state_str << state;
		cdev_sysfs.write(tc_state_dev.str(), state_str.str());
	} else
		curr_state = 0;

}

int cthd_cdev::get_curr_state()
{
	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/cur_state";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::string state_str;
		cdev_sysfs.read(tc_state_dev.str(), state_str);
		std::istringstream(state_str) >> curr_state;
	} else
		curr_state = 0;

	return curr_state;
}

void cthd_cdev::thd_cdev_set_state(int set_point, int temperature, int state)
{
	thd_log_debug(">>thd_cdev_set_state index:%d state:%d\n", index, state);

#ifdef SIMULATE
	thd_log_debug("thd_cdev_set_current state %d max state %d\n", curr_state, max_state);
	if (state) {
		if (curr_state < max_state) {
			++curr_state;
			thd_log_debug("device:%s %d\n", type_str.c_str(), curr_state);
		}
	} else {
		if (curr_state > 0) {
			--curr_state;
			thd_log_debug("device:%s %d\n", type_str.c_str(), curr_state);
		}
	}
#else
	curr_state = get_curr_state();
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

#endif
	thd_log_debug("<<thd_cdev_set_state %d\n", state);
}
