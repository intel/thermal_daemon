/*
 * cthd_cdev_spel.cpp: thermal cooling class implementation	using
 * Qualcomm SPEL
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Author Name <priyansh.jain@oss.qualcomm.com>
 *
 */
#include "thd_cdev_spel.h"
#include "thd_engine.h"

/* This uses Qualcomm SPEL driver to cool the system. SPEL driver shows
 * max thermal spec power in max_state. Each state can compensate
 * spel_power_dec_percent, from the max state.
 *
 * SPEL supports PL1 (long-term) power limiting for SOC and SYS domains.
 * This implementation focuses on set_curr_state functionality as requested.
 */
void cthd_sysfs_cdev_spel::set_curr_state(int state, int control) {
	int new_state = state, ret;

	// If request to set a state which less than max_state i.e. lowest spel power limit
	// then limit to the max_state.
	if (state < max_state)
		new_state = max_state;

	// If the state is more or equal to min_state, means that more than the
	// max spel power limit, restore the power limit to min_state or
	// whatever the power on limit, and restore the power on time window.
	if (new_state >= min_state) {
		// When PPCC is configured (ppcc_max_pwr > 0), always restore to min_state (PPCC max)
		// to ensure we don't exceed PPCC constraints
		if (ppcc_max_pwr > 0) {
			new_state = min_state;  // min_state is set to ppcc_max_pwr from PPCC
		} else if (power_on_constraint_pwr) {
			new_state = power_on_constraint_pwr;
		} else {
			new_state = min_state;
		}

		curr_state = min_state;

		// Restore time window: use PPCC max if configured, otherwise use boot value
		if (ppcc_max_window > 0)
			spel_update_time_window(ppcc_max_window);
		else
			spel_update_time_window(power_on_constraint_time_window);

		constrained = false;
	} else if (control) {
		if (!constrained) {

			// If it is the first time to activate this device, set the time window.
			if (ppcc_min_window)
				spel_update_time_window(ppcc_min_window);

			constrained = true;
		}
	}
	thd_log_info("SPEL %s: set cdev state index %d state %d wr:%d\n",
			domain_type.c_str(), index, state, new_state);

	ret = spel_update_power_limit(new_state);
	if (ret < 0)
		thd_log_info("powercap SPEL %s is unable to update power limits\n",
					domain_type.c_str());

	curr_state = new_state;
}

void cthd_sysfs_cdev_spel::set_curr_state_raw(int state, int arg) {
	set_curr_state(state, arg);
}

// Return the last state or power set during set_curr_state
int cthd_sysfs_cdev_spel::get_curr_state() {
	return curr_state;
}

int cthd_sysfs_cdev_spel::get_max_state() {
	return max_state;
}

int cthd_sysfs_cdev_spel::spel_sysfs_valid()
{
	std::ostringstream temp_str;

	// constraint_index is already set by constructor (0-3 for pl1-pl4)
	// Just verify this constraint exists
	temp_str << "constraint_" << constraint_index << "_name";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_info("powercap SPEL %s: constraint_%d does not exist\n",
				domain_type.c_str(), constraint_index);
		return THD_ERROR;
	}

	// Verify power limit file exists
	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_power_limit_uw";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_debug("powercap SPEL %s: no power limit uw %s\n",
				domain_type.c_str(), temp_str.str().c_str());
		return THD_ERROR;
	}

	// Verify time window file exists
	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_time_window_us";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_info("powercap SPEL %s: no time_window_us %s\n",
				domain_type.c_str(), temp_str.str().c_str());
		return THD_ERROR;
	}

	return 0;
}

int cthd_sysfs_cdev_spel::spel_read_power_limit_max()
{
	std::ostringstream temp_power_str;
	int current_power_limit_max;

	temp_power_str << "constraint_" << constraint_index << "_max_power_uw";
	if (cdev_sysfs.read(temp_power_str.str(), &current_power_limit_max) > 0) {
		return current_power_limit_max;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_spel::spel_read_power_limit()
{
	std::ostringstream temp_power_str;
	int current_power_limit;

	temp_power_str << "constraint_" << constraint_index << "_power_limit_uw";
	if (cdev_sysfs.read(temp_power_str.str(), &current_power_limit) > 0) {
		return current_power_limit;
	}

	return THD_ERROR;
}



int cthd_sysfs_cdev_spel::spel_read_time_window()
{
	std::ostringstream temp_time_str;
	int tm_window;

	temp_time_str << "constraint_" << constraint_index << "_time_window_us";
	if (cdev_sysfs.read(temp_time_str.str(), &tm_window) > 0) {
		return tm_window;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_spel::spel_update_time_window(int time_window)
{
	std::ostringstream temp_time_str;

	temp_time_str << "constraint_" << constraint_index << "_time_window_us";

	if (cdev_sysfs.write(temp_time_str.str(), time_window) <= 0) {
		thd_log_info(
				"SPEL %s: powercap SPEL time window failed to write %d\n",
				domain_type.c_str(), time_window);
		return THD_ERROR;
	}

	thd_log_debug("SPEL %s: time window set to %d us\n",
			domain_type.c_str(), time_window);
	return THD_SUCCESS;
}

int cthd_sysfs_cdev_spel::spel_read_enable_status()
{
	std::ostringstream temp_str;
	int enable;

	temp_str << "enabled";
	if (cdev_sysfs.read(temp_str.str(), &enable) > 0) {
		return enable;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_spel::update() {
	bool ppcc = false;
	int ret;

	if (spel_sysfs_valid())
		return THD_ERROR;

	register_for_restoration();

	// Try to read PPCC power limits from configuration
	ppcc = read_ppcc_power_limits();

	if (ppcc) {
		// This is a PPCC-configured platform with defined power and time window limits
		thd_log_info("SPEL %s: Using PPCC configuration\n", domain_type.c_str());

		// Set increment/decrement values based on PPCC step size
		set_inc_value(-ppcc_step_pwr);
		set_dec_value(-ppcc_step_pwr);

		min_state = ppcc_max_pwr;
		max_state = ppcc_min_pwr;

		power_on_constraint_pwr = spel_read_power_limit();
		power_on_constraint_time_window = spel_read_time_window();
		power_on_enable_status = spel_read_enable_status();

		// Align HW with software min_state at init, similar to RAPL behavior.
		ret = spel_update_power_limit(min_state);
		if (ret < 0) {
			thd_log_info("SPEL %s: failed to initialize power limit to %d\n",
					domain_type.c_str(), min_state);
			return THD_ERROR;
		}

		if (ppcc_max_window > ppcc_min_window) {
			ret = spel_update_time_window(ppcc_max_window);
			if (ret < 0) {
				thd_log_info("SPEL %s: failed to initialize time window to %d\n",
						domain_type.c_str(), ppcc_max_window);
				return THD_ERROR;
			}
		}

		thd_log_info("SPEL %s: PPCC configured - max_pwr:%d min_pwr:%d time_window:%d\n",
				domain_type.c_str(), ppcc_max_pwr, ppcc_min_pwr, ppcc_max_window);
	} else {
        thd_log_info("SPEL %s: PPCC not found, skipping SPEL cdev\n",
                     domain_type.c_str());
        return THD_ERROR;
	}

	curr_state = min_state;

	thd_log_debug("SPEL %s: power_on_enable_status: %d\n",
			domain_type.c_str(), power_on_enable_status);
	thd_log_debug("SPEL %s: power_on_constraint_time_window: %d\n",
			domain_type.c_str(), power_on_constraint_time_window);
	thd_log_debug("SPEL %s: max limit %d increment: %d\n",
			domain_type.c_str(), max_state, inc_dec_val);

	thd_log_info("SPEL %s domain initialized successfully\n", domain_type.c_str());
	return THD_SUCCESS;
}

void cthd_sysfs_cdev_spel::thd_cdev_set_min_state_param(int arg) {
	min_state = curr_state = arg;
}

bool cthd_sysfs_cdev_spel::read_ppcc_power_limits() {
	ppcc_t *ppcc;

	ppcc = thd_engine->get_ppcc_param(device_name);
	if (ppcc) {
		thd_log_info("Reading PPCC from the thermal-conf.xml for SPEL %s\n", domain_type.c_str());

		// Power limits: XML is in mW, convert to uW (multiply by 1000)
		ppcc_max_pwr = ppcc->power_limit_max * 1000;
		ppcc_min_pwr = ppcc->power_limit_min * 1000;
		ppcc_step_pwr = ppcc->step_size * 1000;

		// Time windows: XML is in ms, convert to us for *_time_window_us sysfs.
		ppcc_min_window = ppcc->time_wind_min * 1000;
		ppcc_max_window = ppcc->time_wind_max * 1000;

		if (ppcc_max_pwr <= ppcc_min_pwr) {
			thd_log_info("SPEL %s: Invalid PPCC limits max:%d min:%d min_win:%d step:%d\n",
					domain_type.c_str(), ppcc_max_pwr, ppcc_min_pwr, ppcc_min_window, ppcc_step_pwr);
			return false;
		}

		thd_log_info("SPEL %s: PPCC limits max:%d min:%d min_win:%d max_win:%d step:%d\n",
				domain_type.c_str(), ppcc_max_pwr, ppcc_min_pwr, ppcc_min_window, ppcc_max_window, ppcc_step_pwr);

		return true;
	}

	return false;
}

int cthd_sysfs_cdev_spel::spel_update_power_limit(int power_limit)
{
	std::ostringstream temp_power_str;
	int ret;

	temp_power_str << "constraint_" << constraint_index << "_power_limit_uw";
	ret = cdev_sysfs.write(temp_power_str.str(), power_limit);
	if (ret <= 0) {
		thd_log_info(
				"SPEL %s: powercap SPEL power limit failed to write %d\n",
				domain_type.c_str(), power_limit);
		return ret;
	}

	thd_log_debug("SPEL %s: power limit set to %d uW\n",
			domain_type.c_str(), power_limit);
	return THD_SUCCESS;
}
