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
	unsigned long phy_max;
	int package_id;
	int constraint_index;
	bool dynamic_phy_max_enable;

	virtual bool calculate_phy_max();

public:
	static const int rapl_no_time_windows = 6;
	static const long def_rapl_time_window = 1000000; // micro seconds
	static const unsigned int rapl_min_default_step = 500000; //0.5W

	static const int rapl_low_limit_percent = 25;
	static const int rapl_power_dec_percent = 5;

	cthd_sysfs_cdev_rapl(unsigned int _index, int package) :
			cthd_cdev(_index,
					"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/"), phy_max(
					0), package_id(package), constraint_index(0), dynamic_phy_max_enable(
					false) {
	}
	virtual void set_curr_state(int state, int arg);
	virtual int get_curr_state();
	virtual int get_max_state();
	virtual int update();
	virtual void set_curr_state_raw(int state, int arg);
};

#endif /* THD_CDEV_RAPL_H_ */
