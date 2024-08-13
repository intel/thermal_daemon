/*
 * cthd_sysfs_gen_sysfs.h: thermal cooling class interface
 *	for non thermal cdev sysfs
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#ifndef THD_CDEV_GEN_SYSFS_H_
#define THD_CDEV_GEN_SYSFS_H_

#include "thd_cdev.h"

class cthd_gen_sysfs_cdev: public cthd_cdev {
protected:

public:
	cthd_gen_sysfs_cdev(unsigned int _index, std::string control_path) :
			cthd_cdev(_index, std::move(control_path)) {
	}
	virtual void set_curr_state(int state, int arg);
	virtual void set_curr_state_raw(int state, int arg);
	virtual int update();
};

#endif /* THD_CDEV_GEN_SYSFS_H_ */
