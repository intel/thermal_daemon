/*
 * cthd_sysfs_cdev.cpp: thermal cooling class implementation
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
#include "thd_cdev_therm_sys_fs.h"
#include "thd_engine.h"

/* This uses ACPI style thermal sysfs interface to set states.
 * It expects, max_state. curr_state in thermal sysfs and uses
 * these sysfs-files to control.
 *
 */

int cthd_sysfs_cdev::update() {

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
		if (type_str.size()) {
			// They essentially change same ACPI object, so reading their
			// state from sysfs after a change to any processor will cause
			// double compensation
			if (type_str == "Processor")
				read_back = false;
		}
	}

	thd_log_debug("cooling dev %d:%d:%d:%s\n", index, curr_state, max_state,
			type_str.c_str());

	return THD_SUCCESS;
}

int cthd_sysfs_cdev::get_max_state() {

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

void cthd_sysfs_cdev::set_curr_state(int state, int arg) {

	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/cur_state";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::stringstream state_str;
		state_str << state;
		thd_log_debug("set cdev state index %d state %d\n", index, state);
		cdev_sysfs.write(tc_state_dev.str(), state_str.str());
		curr_state = state;
	} else
		curr_state = 0;
}

int cthd_sysfs_cdev::get_curr_state() {

	if (!read_back) {
		return curr_state;
	}
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
