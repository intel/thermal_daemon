/*
 * cthd_cdev_rapl.cpp: thermal cooling class implementation
 *	using RAPL
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
#include "thd_cdev_rapl.h"
#include "thd_engine.h"

/* This uses Intel RAPL driver to cool the system. RAPL driver show
 * mas thermal spec power in max_state. Each state can compensate
 * rapl_power_dec_percent, from the max state.
 *
 */
void cthd_sysfs_cdev_rapl::set_curr_state(int state, int control) {

	std::stringstream tc_state_dev;

	std::stringstream state_str;
	int new_state = state, ret;

	if (bios_locked) {
		if (state <= inc_dec_val)
			curr_state = min_state;
		else
			curr_state = max_state;

		return;
	}

	// If request to set a state which less than max_state i.e. lowest rapl power limit
	// then limit to the max_state.
	if (state < max_state)
		new_state = max_state;

	// If the state is more or equal to min_state, means that more than the
	// max rapl power limit, restore the power limit to min_state or
	// whatever the power on limit. Also make the rapl limit enforcement
	// to disabled, also restore power on time window.
	if (new_state >= min_state) {
		if (power_on_constraint_0_pwr)
			new_state = power_on_constraint_0_pwr;
		else
			new_state = min_state;

		curr_state = min_state;

		// If disabled during power on, disable
		if (!power_on_enable_status)
			rapl_update_enable_status(0);

		rapl_update_time_window(power_on_constraint_0_time_window);

		constrained = false;
	} else if (control) {
		if (!constrained) {

			// If it is the first time to activate this device, set the enabled flag
			// and set the time window.
			rapl_update_time_window(def_rapl_time_window);

			// Set enable flag only if it was disabled
			if (!power_on_enable_status)
				rapl_update_enable_status(1);

			constrained = true;
		}
	}
	thd_log_info("set cdev state index %d state %d wr:%d\n", index, state,
			new_state);

	ret = rapl_update_pl1(new_state);
	if (ret < 0) {
		curr_state = (state == 0) ? 0 : max_state;
		if (ret == -ENODATA) {
			thd_log_info("powercap RAPL is BIOS locked, cannot update\n");
			bios_locked = true;
		}
	}
	curr_state = new_state;
}

void cthd_sysfs_cdev_rapl::set_curr_state_raw(int state, int arg) {
	set_curr_state(state, arg);
}

// Return the last state or power set during set_curr_state
int cthd_sysfs_cdev_rapl::get_curr_state() {
	return curr_state;
}

// Return the current power, using this the controller can choose the next state
int cthd_sysfs_cdev_rapl::get_curr_state(bool read_again) {
	if (dynamic_phy_max_enable) {
		thd_engine->rapl_power_meter.rapl_start_measure_power();
		return thd_engine->rapl_power_meter.rapl_action_get_power(PACKAGE);
	}
	return curr_state;
}

int cthd_sysfs_cdev_rapl::get_max_state() {

	return max_state;
}

int cthd_sysfs_cdev_rapl::rapl_sysfs_valid()
{
	std::stringstream temp_str;
	int found = 0;
	int i;

	// The primary control is powercap rapl long_term control
	// If absent we can't use rapl cooling device
	for (i = 0; i < rapl_no_time_windows; ++i) {
		temp_str << "constraint_" << i << "_name";
		if (cdev_sysfs.exists(temp_str.str())) {
			std::string type_str;
			cdev_sysfs.read(temp_str.str(), type_str);
			if (type_str == "long_term") {
				constraint_index = i;
				found = 1;
			}
		}
	}

	if (!found) {
		thd_log_info("powercap RAPL no long term time window\n");
		return THD_ERROR;
	}

	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_power_limit_uw";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_debug("powercap RAPL no  power limit uw %s \n",
				temp_str.str().c_str());
		return THD_ERROR;
	}

	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_time_window_us";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_info("powercap RAPL no time_window_us %s \n",
				temp_str.str().c_str());
		return THD_ERROR;
	}

	return 0;
}

int cthd_sysfs_cdev_rapl::rapl_read_pl1_max()
{
	std::stringstream temp_power_str;
	int current_pl1_max;

	temp_power_str << "constraint_" << constraint_index << "_max_power_uw";
	if (cdev_sysfs.read(temp_power_str.str(), &current_pl1_max) > 0) {
		return current_pl1_max;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_rapl::rapl_read_pl1()
{
	std::stringstream temp_power_str;
	int current_pl1;

	temp_power_str << "constraint_" << constraint_index << "_power_limit_uw";
	if (cdev_sysfs.read(temp_power_str.str(), &current_pl1) > 0) {
		return current_pl1;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_rapl::rapl_update_pl1(int pl1)
{
	std::stringstream temp_power_str;
	int ret;

	temp_power_str << "constraint_" << constraint_index << "_power_limit_uw";
	ret = cdev_sysfs.write(temp_power_str.str(), pl1);
	if (ret <= 0) {
		thd_log_info(
				"pkg_power: powercap RAPL max power limit failed to write %d \n",
				pl1);
		return ret;
	}

	return THD_SUCCESS;
}

int cthd_sysfs_cdev_rapl::rapl_read_time_window()
{
	std::stringstream temp_time_str;
	int tm_window;

	temp_time_str << "constraint_" << constraint_index << "_time_window_us";
	if (cdev_sysfs.read(temp_time_str.str(), &tm_window) > 0) {
		return tm_window;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_rapl::rapl_update_time_window(int time_window)
{
	std::stringstream temp_time_str;

	temp_time_str << "constraint_" << constraint_index << "_time_window_us";

	if (cdev_sysfs.write(temp_time_str.str(), time_window) <= 0) {
		thd_log_info(
				"pkg_power: powercap RAPL time window failed to write %d \n",
				time_window);
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_sysfs_cdev_rapl::rapl_update_enable_status(int enable)
{
	std::stringstream temp_str;

	temp_str << "enabled";
	if (cdev_sysfs.write(temp_str.str(), enable) <= 0) {
		thd_log_info(
				"pkg_power: powercap RAPL enable failed to write %d \n",
				enable);
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_sysfs_cdev_rapl::rapl_read_enable_status()
{
	std::stringstream temp_str;
	int enable;

	temp_str << "enabled";
	if (cdev_sysfs.read(temp_str.str(), &enable) > 0) {
		return enable;
	}

	return THD_ERROR;

}

void cthd_sysfs_cdev_rapl::set_tcc(int tcc) {
	csys_fs sysfs("/sys/bus/pci/devices/0000:00:04.0/");

	if (!sysfs.exists("tcc_offset_degree_celsius"))
		return;

	sysfs.write("tcc_offset_degree_celsius", tcc);
}

void cthd_sysfs_cdev_rapl::set_adaptive_target(struct adaptive_target target) {
	int argument = std::stoi(target.argument, NULL);
	if (target.code == "PL1MAX") {
		set_curr_state(argument * 1000, 1);
	} else if (target.code == "PL1TimeWindow") {
		rapl_update_time_window(argument * 1000);
	} else if (target.code == "TccOffset") {
		set_tcc(argument);
	}
}

int cthd_sysfs_cdev_rapl::update() {
	std::stringstream temp_str;
	int constraint_phy_max;
	bool ppcc = false;
	std::string domain_name;

	if (rapl_sysfs_valid())
		return THD_ERROR;

	ppcc = read_ppcc_power_limits();

	if (ppcc) {
		// This is a DPTF compatible platform, which defined
		// maximum and minimum power limits. We can trust this to be something sane.
		phy_max = pl0_max_pwr;
		// We want to be aggressive controlling temperature but lazy during
		// removing of controls
		set_inc_value(-pl0_step_pwr * 2);
		set_dec_value(-pl0_step_pwr);
		min_state = pl0_max_pwr;
		max_state = pl0_min_pwr;

		rapl_update_pl1(pl0_max_pwr);

		// To be efficient to control from the current power instead of PPCC max.
		thd_engine->rapl_power_meter.rapl_start_measure_power();
		dynamic_phy_max_enable = true;
		//set_debounce_interval(1);

		// By default enable the rapl device to enforce any power limits
		rapl_update_enable_status(1);

	} else {

		// This is not a DPTF platform
		// Read the max power from the powercap rapl sysfs
		// If not present, we can't use rapl to cool.
		phy_max = rapl_read_pl1_max();

		// Check if there is any sane max power limit set
		if (cdev_sysfs.read(temp_str.str(), &phy_max) < 0 || phy_max < 0
				|| phy_max > rapl_max_sane_phy_max) {
			thd_log_info("%s:powercap RAPL invalid max power limit range \n",
					domain_name.c_str());
			thd_log_info("Calculate dynamically phy_max \n");

			power_on_constraint_0_pwr = rapl_read_pl1();
			thd_log_debug("power_on_constraint_0_pwr %d\n",
					power_on_constraint_0_pwr);

			power_on_constraint_0_time_window = rapl_read_time_window();
			thd_log_debug("power_on_constraint_0_time_window %d\n",
					power_on_constraint_0_time_window);

			phy_max = max_state = 0;
			curr_state = min_state = rapl_max_sane_phy_max;
			thd_engine->rapl_power_meter.rapl_start_measure_power();
			set_inc_dec_value(-rapl_min_default_step);
			dynamic_phy_max_enable = true;

			return THD_SUCCESS;
		}

		constraint_phy_max = rapl_read_pl1();

		// If the constraint_0_max_power_uw < constraint_0_power_limit_uw
		// Use constraint_0_power_limit_uw as the phy_max and min_state
		if (constraint_phy_max > phy_max) {
			thd_log_info(
					"Default constraint power limit is more than max power %d:%d\n",
					constraint_phy_max, phy_max);
			phy_max = constraint_phy_max;
		}
		thd_log_info("powercap RAPL max power limit range %d \n", phy_max);

		set_inc_dec_value(-phy_max * (float) rapl_power_dec_percent / 100);
		min_state = phy_max;
		max_state = min_state
				- (float) min_state * rapl_low_limit_percent / 100;
	}

	power_on_constraint_0_time_window = rapl_read_time_window();
	power_on_enable_status = rapl_read_enable_status();

	thd_log_debug("power_on_enable_status: %d\n", power_on_enable_status);

	thd_log_debug("RAPL max limit %d increment: %d\n", max_state, inc_dec_val);

	set_pid_param(-1000, 100, 10);
	curr_state = min_state;

	return THD_SUCCESS;
}

bool cthd_sysfs_cdev_rapl::read_ppcc_power_limits() {
	csys_fs sys_fs;
	ppcc_t *ppcc;

	ppcc = thd_engine->get_ppcc_param(device_name);
	if (ppcc) {
		thd_log_info("Reading PPCC from the thermal-conf.xml\n");
		pl0_max_pwr = ppcc->power_limit_max * 1000;
		pl0_min_pwr = ppcc->power_limit_min * 1000;
		pl0_min_window = ppcc->time_wind_min * 1000;
		pl0_step_pwr = ppcc->step_size * 1000;
		return true;
	}

	if (sys_fs.exists("/sys/bus/pci/devices/0000:00:04.0/power_limits/"))
		sys_fs.update_path("/sys/bus/pci/devices/0000:00:04.0/power_limits/");
	else if (sys_fs.exists("/sys/bus/pci/devices/0000:00:0b.0/power_limits/"))
		sys_fs.update_path("/sys/bus/pci/devices/0000:00:0b.0/power_limits/");
	else if (sys_fs.exists(
			"/sys/bus/platform/devices/INT3401:00/power_limits/"))
		sys_fs.update_path(
				"/sys/bus/platform/devices/INT3401:00/power_limits/");
	else
		return false;

	if (sys_fs.exists("power_limit_0_max_uw")) {
		if (sys_fs.read("power_limit_0_max_uw", &pl0_max_pwr) <= 0)
			return false;
	}

	if (sys_fs.exists("power_limit_0_min_uw")) {
		if (sys_fs.read("power_limit_0_min_uw", &pl0_min_pwr) <= 0)
			return false;
	}

	if (sys_fs.exists("power_limit_0_tmin_us")) {
		if (sys_fs.read("power_limit_0_tmin_us", &pl0_min_window) <= 0)
			return false;
	}

	if (sys_fs.exists("power_limit_0_step_uw")) {
		if (sys_fs.read("power_limit_0_step_uw", &pl0_step_pwr) <= 0)
			return false;
	}

	if (pl0_max_pwr && pl0_min_pwr && pl0_min_window && pl0_step_pwr) {
		thd_log_debug("ppcc limits max:%u min:%u  min_win:%u step:%u\n",
				pl0_max_pwr, pl0_min_pwr, pl0_min_window, pl0_step_pwr);
		return true;
	}

	return false;
}

void cthd_sysfs_cdev_rapl::thd_cdev_set_min_state_param(int arg) {
	min_state = curr_state = arg;
}
