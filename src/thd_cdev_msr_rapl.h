/*
 * thd_cdev_mar_rapl.h: thermal cooling class interface
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
#ifndef THD_CDEV_RAPL_MSR_H_
#define THD_CDEV_RAPL_MSR_H_

#include "thd_cdev.h"
#include "thd_rapl_interface.h"

class cthd_cdev_rapl_msr: public cthd_cdev {
protected:
	int max_state;
	c_rapl_interface rapl;

	double thermal_spec_power;
	double max_power;
	double min_power;
	double max_time_window;
	int phy_max;
	bool control_start;

public:
	static const int rapl_low_limit_percent = 25;
	static const int rapl_power_dec_percent = 5;

	cthd_cdev_rapl_msr(unsigned int _index, int _cpu_index) :
			cthd_cdev(_index, "/sys/devices/system/cpu/"), max_state(0), rapl(
					0), thermal_spec_power(0.0), max_power(0.0), min_power(0.0), max_time_window(
					0.0), phy_max(0), control_start(false) {
	}
	void set_curr_state(int state, int arg);
	int get_max_state();
	virtual int update();
};

#endif /* THD_CDEV_TSTATES_H_ */
