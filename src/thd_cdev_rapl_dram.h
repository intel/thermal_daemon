/*
 * cthd_cdev_rapl_dram.h: thermal cooling class interface
 *	using RAPL DRAM
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
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

#ifndef THD_CDEV_RAPL_DRAM_H_
#define THD_CDEV_RAPL_DRAM_H_

#include "thd_cdev_rapl.h"

class cthd_sysfs_cdev_rapl_dram: public cthd_sysfs_cdev_rapl {
private:

public:
	cthd_sysfs_cdev_rapl_dram(unsigned int _index, int _package) :
			cthd_sysfs_cdev_rapl(_index, _package) {
		device_name = "TMEM.D0";
	}

	virtual int update();
};

#endif /* THD_CDEV_RAPL_DRAM_H_ */
