/*
 * cthd_cdev_rapl.h: thermal cooling class interface
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

#ifndef THD_CDEV_RAPL_H_
#define THD_CDEV_RAPL_H_

#include "thd_cdev.h"
#include "thd_sys_fs.h"

class cthd_sysfs_cdev_rapl: public cthd_cdev {
protected:
	int phy_max;
	int package_id;
	int constraint_index;
	int pl2_index;
	bool dynamic_phy_max_enable;
	int pl0_max_pwr;
	int pl0_min_pwr;
	int pl0_min_window;
	int pl0_max_window;
	int pl0_step_pwr;
	int pl1_max_pwr;
	int pl1_min_pwr;
	int pl1_min_window;
	int pl1_max_window;
	int pl1_step_pwr;
	int pl1_valid;
	bool bios_locked;
	bool constrained;
	int power_on_constraint_0_pwr;
	int power_on_constraint_0_time_window;
	int power_on_enable_status;
	std::string device_name;
	virtual bool read_ppcc_power_limits();

private:
	int rapl_sysfs_valid();
	int rapl_read_pl1();
	int rapl_read_pl1_max();
	int rapl_update_pl1(int pl1);
	int rapl_read_pl2();
	int rapl_update_pl2(int pl2);
	int rapl_read_time_window();
	int rapl_update_time_window(int time_window);
	int rapl_update_pl2_time_window(int time_window);
	int rapl_read_enable_status();

public:
	static const int rapl_no_time_windows = 6;
	static const long def_rapl_time_window = 1000000; // micro seconds
	static const int rapl_min_default_step = 500000; //0.5W
	static const int rapl_max_sane_phy_max = 100000000; // Some sane very high value in uW

	static const int rapl_low_limit_percent = 50;
	static const int rapl_power_dec_percent = 5;

	cthd_sysfs_cdev_rapl(unsigned int _index, int package) :
			cthd_cdev(_index,
					"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/"), phy_max(
					0), package_id(package), constraint_index(
					0), pl2_index(-1), dynamic_phy_max_enable(
					false), pl0_max_pwr(0), pl0_min_pwr(0), pl0_min_window(
					0), pl0_max_window(0), pl0_step_pwr(
					0), bios_locked(false), constrained(
					false), power_on_constraint_0_pwr(0), power_on_constraint_0_time_window(
					0), power_on_enable_status(0), device_name("TCPU.D0")
	{
		pl1_max_pwr = 0;
		pl1_min_pwr = 0;
		pl1_min_window = 0;
		pl1_max_window = 0;
		pl1_step_pwr = 0;
		pl1_valid = 0;
	}
	cthd_sysfs_cdev_rapl(unsigned int _index, int package,
			std::string contol_path) :
			cthd_cdev(_index, std::move(contol_path)), phy_max(0), package_id(
					package), constraint_index(
					0), pl2_index(
					-1), dynamic_phy_max_enable(false), pl0_max_pwr(
					0), pl0_min_pwr(
					0), pl0_min_window(0), pl0_max_window(0), pl0_step_pwr(0), bios_locked(
					false), constrained(
					false), power_on_constraint_0_pwr(0), power_on_constraint_0_time_window(
					0), power_on_enable_status(0), device_name("TCPU.D0")
	{
		pl1_max_pwr = 0;
		pl1_min_pwr = 0;
		pl1_min_window = 0;
		pl1_max_window = 0;
		pl1_step_pwr = 0;
		pl1_valid = 0;
	}

	virtual void set_curr_state(int state, int arg);
	virtual int get_curr_state();
	virtual int get_curr_state(bool read_again);
	virtual int get_max_state();
	virtual int update();
	virtual void set_curr_state_raw(int state, int arg);
	void set_tcc(int tcc);
	void set_adaptive_target(struct adaptive_target &target);
	void thd_cdev_set_min_state_param(int arg);
	int get_phy_max_state() {
		return phy_max;
	}
	int rapl_update_enable_status(int enable);
};

#endif /* THD_CDEV_RAPL_H_ */
