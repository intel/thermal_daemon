/*
 * thd_cdev_rapl_msr.cpp: thermal cooling class implementation
 *	using RAPL MSRs.
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

#include <string>
#include <sstream>
#include <vector>
#include "thd_cdev_msr_rapl.h"

void cthd_cdev_rapl_msr::set_curr_state(int state, int arg) {
	int new_state;

	if (!control_start && state == inc_dec_val) {
		thd_log_debug("rapl state control begin\n");
		rapl.store_pkg_power_limit();
		control_start = true;
	}
	if (state < inc_dec_val) {
		new_state = 0;
		curr_state = 0;
	} else {
		new_state = phy_max - state;
		curr_state = state;
		thd_log_debug("rapl state = %d new_state = %d\n", state, new_state);
		rapl.set_pkg_power_limit(1, new_state); //thd_engine->def_poll_interval/1000 - 1, new_state);
	}

	if (state < inc_dec_val) {
		thd_log_debug("rapl state control end\n");
		rapl.restore_pkg_power_limit();
		control_start = false;
	}
}

int cthd_cdev_rapl_msr::get_max_state() {

	return max_state;
}

int cthd_cdev_rapl_msr::update() {
	int ret;

	if (!rapl.pkg_domain_present())
		return THD_ERROR;

	if (!rapl.pp0_domain_present())
		return THD_ERROR;

	if (rapl.get_power_unit() < 0)
		return THD_ERROR;

	if (rapl.get_time_unit() < 0)
		return THD_ERROR;

	ret = rapl.get_pkg_power_info(&thermal_spec_power, &max_power, &min_power,
			&max_time_window);
	if (ret < 0)
		return ret;
	thd_log_debug(
			"Pkg Power Info: Thermal spec %f watts, max %f watts, min %f watts, max time window %f seconds\n",
			thermal_spec_power, max_power, min_power, max_time_window);

	max_state = 0;

	if (thermal_spec_power > 0)
		phy_max = (int) thermal_spec_power * 1000; // change to milliwatts
	else if (max_power > 0)
		phy_max = (int) max_power * 1000; // // change to milliwatts
	else
		return THD_ERROR;

	set_inc_dec_value(phy_max * (float) rapl_power_dec_percent / 100);

	if (inc_dec_val == 0)
		set_inc_dec_value(phy_max * (float) rapl_power_dec_percent * 2 / 100);

	if (inc_dec_val == 0) // power limit is too small
		inc_dec_val = 1;

	max_state = phy_max - ((float) phy_max * rapl_low_limit_percent / 100);

	thd_log_debug("RAPL phy_max %d max_state %d inc_dec %d \n", phy_max,
			max_state, inc_dec_val);

	return THD_SUCCESS;
}
