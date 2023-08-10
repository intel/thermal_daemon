/*
 * cthd_sysfs_gen_sysfs.cpp: thermal cooling class interface
 *	for non thermal cdev sysfs
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "thd_cdev_gen_sysfs.h"

int cthd_gen_sysfs_cdev::update() {
	if (cdev_sysfs.exists()) {
		std::string state_str;
		cdev_sysfs.read("", state_str);
		std::istringstream(state_str) >> curr_state;
		min_state = max_state = curr_state;
	} else {
		if (cdev_sysfs.get_base_path() && !strcmp(cdev_sysfs.get_base_path(), ""))
			return THD_ERROR;

		int ret = cdev_sysfs.create();

		if (ret < 0)
			return THD_ERROR;

		ret = cdev_sysfs.write("", 0);
		if (ret < 0)
			return THD_ERROR;
	}

	return THD_SUCCESS;
}

void cthd_gen_sysfs_cdev::set_curr_state(int state, int arg) {

	std::stringstream state_str;

	if (write_prefix.length())
		state_str << write_prefix;

	state_str << state;
	thd_log_debug("set cdev state index %d state %d %s\n", index, state,
			state_str.str().c_str());
	cdev_sysfs.write("", state_str.str());
	curr_state = state;
}

void cthd_gen_sysfs_cdev::set_curr_state_raw(int state, int arg) {
	set_curr_state(state, arg);
}
