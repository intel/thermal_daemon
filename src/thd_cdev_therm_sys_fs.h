/*
 * cthd_sysfs_cdev.cpp: thermal cooling class interface
 *	for thermal sysfs
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

#ifndef THD_CDEV_THERM_SYS_FS_H_
#define THD_CDEV_THERM_SYS_FS_H_

#include "thd_cdev.h"

class cthd_sysfs_cdev: public cthd_cdev
{
private:
	bool use_custom_cdevs;
	std::string custom_path_str;

public:
	cthd_sysfs_cdev(unsigned int _index, std::string control_path): cthd_cdev
	(_index, control_path), use_custom_cdevs(false){}
	void set_curr_state(int state, int arg);
	int get_curr_state();
	int get_max_state();
	int update();
};

#endif /* THD_CDEV_THERM_SYS_FS_H_ */
