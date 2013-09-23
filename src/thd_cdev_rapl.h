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

#include "thd_cdev_therm_sys_fs.h"

class cthd_sysfs_cdev_rapl: public cthd_sysfs_cdev
{
private:
	unsigned long phy_max;
	int package_id;
	int constraint_index;

public:
	static const int rapl_no_time_windows = 6;
	static const long def_rapl_time_window = 1000000; // micro seconds

	static const int rapl_low_limit_percent = 25;
	static const int rapl_power_dec_percent = 5;

	cthd_sysfs_cdev_rapl(unsigned int _index, std::string control_path, int package):
		cthd_sysfs_cdev(_index, control_path), package_id(package), constraint_index(0) {}
	virtual void set_curr_state(int state, int arg);
	virtual int get_curr_state();
	virtual int get_max_state();
	virtual int update();
};

#endif /* THD_CDEV_RAPL_H_ */
