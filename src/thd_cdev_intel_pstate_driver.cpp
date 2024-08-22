/*
 * thd_sysfs_intel_pstate_driver.cpp: thermal cooling class implementation
 *	using Intel p state driver
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

#include "thd_cdev_intel_pstate_driver.h"

/*
 * This implementation allows controlling get max state and
 * set current state of using Intel P state driver
 * P state drives uses a percent count. 100% means full
 * performance, setting anything lower limits performance.
 * Each lower state reduces performance by unit value.
 * unit value is calculated using number of possible p states
 * . Each step reduces bye one p state.
 *  Contents of "/sys/devices/system/cpu/intel_pstate/"
 max_perf_pct,  min_perf_pct,  no_turbo
 */

void cthd_intel_p_state_cdev::set_curr_state(int state, int arg) {
	std::stringstream tc_state_dev;
	int new_state;

	tc_state_dev << "/max_perf_pct";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::stringstream state_str;
		if (state == 0)
			new_state = 100;
		else {
			new_state = 100 - (state + min_compensation) * unit_value;
		}
		state_str << new_state;
		thd_log_debug("set cdev state index %d state %d percent %d\n", index,
				state, new_state);
		if (new_state <= turbo_disable_percent)
			set_turbo_disable_status(true);
		else
			set_turbo_disable_status(false);
		if (cdev_sysfs.write(tc_state_dev.str(), state_str.str()) < 0)
			curr_state = (state == 0) ? 0 : max_state;
		else
			curr_state = state;
	} else
		curr_state = (state == 0) ? 0 : max_state;
}

void cthd_intel_p_state_cdev::set_turbo_disable_status(bool enable) {
	std::stringstream tc_state_dev;

	if (enable == turbo_status) {
		return;
	}
	tc_state_dev << "/no_turbo";
	if (enable) {
		cdev_sysfs.write(tc_state_dev.str(), "1");
		thd_log_info("turbo disabled\n");
	} else {
		cdev_sysfs.write(tc_state_dev.str(), "0");
		thd_log_info("turbo enabled\n");
	}
	turbo_status = enable;
}

int cthd_intel_p_state_cdev::get_max_state() {
	return max_state;
}

int cthd_intel_p_state_cdev::map_target_state(int target_valid, int target_state) {
	if (!target_valid)
		return target_state;

	if (target_state > 100)
		return 0;

	return (100 - target_state) / unit_value;
}

int cthd_intel_p_state_cdev::update() {
	std::stringstream tc_state_dev;
	std::stringstream status_attr;

	status_attr << "/status";
	if (cdev_sysfs.exists(status_attr.str())) {
		std::string status_str;
		int ret;

		ret = cdev_sysfs.read(status_attr.str(), status_str);
		if (ret >= 0 && status_str != "active") {
			thd_log_info("intel pstate is not in active mode\n");
			return THD_ERROR;
		}
	}

	tc_state_dev << "/max_perf_pct";
	if (cdev_sysfs.exists(tc_state_dev.str())) {
		std::string state_str;
		cdev_sysfs.read(tc_state_dev.str(), state_str);
		std::istringstream(state_str) >> curr_state;
	} else {
		return THD_ERROR;
	}
	thd_log_info("Use Default pstate drv settings\n");
	max_state = default_max_state;
	min_compensation = 0;
	unit_value = 100.0 / max_state;
	curr_state = 0;
	thd_log_debug(
			"cooling dev index:%d, curr_state:%d, max_state:%d, unit:%f, min_com:%d, type:%s\n",
			index, curr_state, max_state, unit_value, min_compensation,
			type_str.c_str());

	return THD_SUCCESS;
}
