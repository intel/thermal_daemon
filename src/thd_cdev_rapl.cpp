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
			if (pl0_min_window)
				rapl_update_time_window(pl0_min_window);
			else
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
	int found_long_term = 0;
	int i;

	// The primary control is powercap rapl long_term control
	// If absent we can't use rapl cooling device
	for (i = 0; i < rapl_no_time_windows; ++i) {
		temp_str.str(std::string());
		temp_str << "constraint_" << i << "_name";
		if (cdev_sysfs.exists(temp_str.str())) {
			std::string type_str;
			cdev_sysfs.read(temp_str.str(), type_str);
			if (type_str == "long_term") {
				constraint_index = i;
				found_long_term = 1;
			}
			if (type_str == "short_term") {
				pl2_index = i;
			}
		}
	}

	if (!found_long_term) {
		thd_log_info("powercap RAPL no long term time window\n");
		return THD_ERROR;
	}

	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_power_limit_uw";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_debug("powercap RAPL no  power limit uw %s\n",
				temp_str.str().c_str());
		return THD_ERROR;
	}

	temp_str.str(std::string());
	temp_str << "constraint_" << constraint_index << "_time_window_us";
	if (!cdev_sysfs.exists(temp_str.str())) {
		thd_log_info("powercap RAPL no time_window_us %s\n",
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
				"pkg_power: powercap RAPL max power limit failed to write %d\n",
				pl1);
		return ret;
	}

	return THD_SUCCESS;
}

int cthd_sysfs_cdev_rapl::rapl_read_pl2()
{
	std::stringstream temp_power_str;
	int current_pl2;

	temp_power_str << "constraint_" << pl2_index << "_power_limit_uw";
	if (cdev_sysfs.read(temp_power_str.str(), &current_pl2) > 0) {
		return current_pl2;
	}

	return THD_ERROR;
}

int cthd_sysfs_cdev_rapl::rapl_update_pl2(int pl2)
{
	std::stringstream temp_power_str;
	int ret;

	if (pl2_index == -1) {
		thd_log_warn("Asked to set PL2 but couldn't find a PL2 device\n");
		return THD_ERROR;
	}
	temp_power_str << "constraint_" << pl2_index << "_power_limit_uw";
	ret = cdev_sysfs.write(temp_power_str.str(), pl2);
	if (ret <= 0) {
		thd_log_info(
				"pkg_power: powercap RAPL max power limit failed to write PL2 %d\n",
				pl2);
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
				"pkg_power: powercap RAPL time window failed to write %d\n",
				time_window);
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_sysfs_cdev_rapl::rapl_update_pl2_time_window(int time_window)
{
	std::stringstream temp_time_str;

	temp_time_str << "constraint_" << pl2_index << "_time_window_us";

	if (cdev_sysfs.write(temp_time_str.str(), time_window) <= 0) {
		thd_log_info(
				"pkg_power: powercap RAPL time window failed to write %d\n",
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
				"pkg_power: powercap RAPL enable failed to write %d\n",
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

	if (sysfs.write("tcc_offset_degree_celsius", tcc) == -1)
		thd_log_debug("TCC write failed\n");
}

void cthd_sysfs_cdev_rapl::set_adaptive_target(struct adaptive_target &target) {
	int argument = std::stoi(target.argument, NULL);
	if (target.code == "PL1MAX") {
		int pl1_rapl;

		min_state = pl0_max_pwr = argument * 1000;

		pl1_rapl = rapl_read_pl1();
		if (curr_state > pl1_rapl)
			set_curr_state(pl1_rapl, 1);

		if (curr_state > min_state)
			set_curr_state(min_state, 1);
	} else if (target.code == "PL1MIN") {
		max_state = pl0_min_pwr = argument * 1000;
		if (curr_state < max_state)
			set_curr_state(max_state, 1);
	} else if (target.code == "PL1STEP") {
		pl0_step_pwr = argument * 1000;
		set_inc_value(-pl0_step_pwr * 2);
		set_dec_value(-pl0_step_pwr);
	} else if (target.code == "PL1TimeWindow") {
		pl0_min_window = argument * 1000;
	} else if (target.code == "PL1PowerLimit") {
		set_curr_state(argument * 1000, 1);
	} else if (target.code == "PL2PowerLimit") {
		rapl_update_pl2(argument * 1000);
	} else if (target.code == "TccOffset") {
		set_tcc(argument);
	}
}

int cthd_sysfs_cdev_rapl::update() {
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

		if (pl0_max_window > pl0_min_window)
			rapl_update_time_window(pl0_max_window);

		// To be efficient to control from the current power instead of PPCC max.
		thd_engine->rapl_power_meter.rapl_start_measure_power();
		dynamic_phy_max_enable = true;
		//set_debounce_interval(1);

		// Some system has PL2 limit as 0, then try to set PL2 limit also
		if (!rapl_read_pl2()) {
			thd_log_info("PL2 power limit is 0, will conditionally enable\n");
			if (pl1_max_pwr) {
				thd_log_info("PL2 limits are updated to %d %d\n", pl1_max_pwr,
						pl1_max_window);
				rapl_update_pl2(pl1_max_pwr);
				rapl_update_pl2_time_window(pl1_max_window);
				rapl_update_enable_status(1);
			}
		} else {
			// By default enable the rapl device to enforce any power limits
			rapl_update_enable_status(1);
		}
	} else {

		// This is not a DPTF platform
		// Read the max power from the powercap rapl sysfs
		// If not present, we can't use rapl to cool.
		phy_max = rapl_read_pl1_max();

		// Check if there is any sane max power limit set
		if (phy_max < 0 || phy_max > rapl_max_sane_phy_max) {
			int ret = cdev_sysfs.read("name", domain_name);

			if (!ret)
				thd_log_info("%s:powercap RAPL invalid max power limit range\n",
						domain_name.c_str());

			thd_log_info("Calculate dynamically phy_max\n");

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
		thd_log_info("powercap RAPL max power limit range %d\n", phy_max);

		set_inc_dec_value(-phy_max * (float) rapl_power_dec_percent / 100);
		min_state = phy_max;
		max_state = min_state
				- (float) min_state * rapl_low_limit_percent / 100;
	}

	power_on_constraint_0_time_window = rapl_read_time_window();
	power_on_enable_status = rapl_read_enable_status();

	thd_log_debug("power_on_enable_status: %d\n", power_on_enable_status);
	thd_log_debug("power_on_constraint_0_time_window: %d\n", power_on_constraint_0_time_window);

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
		int def_max_power;

		thd_log_info("Reading PPCC from the thermal-conf.xml\n");
		pl0_max_pwr = ppcc->power_limit_max * 1000;
		pl0_min_pwr = ppcc->power_limit_min * 1000;
		pl0_min_window = ppcc->time_wind_min * 1000;
		pl0_max_window = ppcc->time_wind_max * 1000;
		pl0_step_pwr = ppcc->step_size * 1000;

		pl1_valid = ppcc->limit_1_valid;
		if (pl1_valid) {
			pl1_max_pwr = ppcc->power_limit_1_max * 1000;
			pl1_min_pwr = ppcc->power_limit_1_min * 1000;
			pl1_min_window = ppcc->time_wind_1_min * 1000;
			pl1_max_window = ppcc->time_wind_1_max * 1000;
			pl1_step_pwr = ppcc->step_1_size * 1000;
		}

		if (pl0_max_pwr <= pl0_min_pwr) {
			thd_log_info("Invalid limits: ppcc limits max:%d min:%d  min_win:%d step:%d\n",
					pl0_max_pwr, pl0_min_pwr, pl0_min_window, pl0_step_pwr);
			return false;
		}

		thd_log_info("ppcc limits max:%d min:%d  min_win:%d step:%d\n",
				pl0_max_pwr, pl0_min_pwr, pl0_min_window, pl0_step_pwr);

		int policy_matched;

		policy_matched = thd_engine->search_idsp("63BE270F-1C11-48FD-A6F7-3AF253FF3E2D");
		if (policy_matched != THD_SUCCESS)
			policy_matched = thd_engine->search_idsp("9E04115A-AE87-4D1C-9500-0F3E340BFE75");

		if (policy_matched == THD_SUCCESS) {
			thd_log_info("IDSP policy matched, so trusting PPCC limits\n");
			return true;
		}

		def_max_power = rapl_read_pl1_max();
		if (def_max_power > pl0_max_pwr)
			thd_log_warn("ppcc limits is less than def PL1 max power :%d check thermal-conf.xml.auto\n", def_max_power);

		return true;
	}

	std::string domain_name;

	// Since this base class is also used by DRAM rapl, avoid reading PPCC as
	// there are no power limits defined by DPTF based systems for any other
	// domain other than package-0
	cdev_sysfs.read("name", domain_name);
	if (domain_name != "package-0")
		return false;

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

	if (sys_fs.exists("power_limit_0_tmax_us")) {
		if (sys_fs.read("power_limit_0_tmax_us", &pl0_max_window) <= 0)
			return false;
	}

	if (sys_fs.exists("power_limit_0_step_uw")) {
		if (sys_fs.read("power_limit_0_step_uw", &pl0_step_pwr) <= 0)
			return false;
	}

	if (pl0_max_pwr && pl0_min_pwr && pl0_min_window && pl0_step_pwr && pl0_max_window) {
		int def_max_power;

		if (pl0_max_pwr <= pl0_min_pwr) {
			thd_log_info("Invalid limits: ppcc limits max:%d min:%d  min_win:%d step:%d\n",
					pl0_max_pwr, pl0_min_pwr, pl0_min_window, pl0_step_pwr);
			return false;
		}

		thd_log_info("ppcc limits max:%d min:%d  min_win:%d step:%d\n",
				pl0_max_pwr, pl0_min_pwr, pl0_min_window, pl0_step_pwr);

		def_max_power = rapl_read_pl1_max();
		if (def_max_power > pl0_max_pwr) {
			thd_log_info("ppcc limits is less than def PL1 max power :%d, so ignore\n", def_max_power);
			return false;
		}

		return true;
	}

	return false;
}

void cthd_sysfs_cdev_rapl::thd_cdev_set_min_state_param(int arg) {
	min_state = curr_state = arg;
}
