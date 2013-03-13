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

public:
	cthd_sysfs_cdev_rapl(unsigned int _index, std::string control_path):
		cthd_sysfs_cdev(_index, control_path) {}
	void set_curr_state(int state, int arg);
	int get_curr_state();
};

#endif /* THD_CDEV_RAPL_H_ */
