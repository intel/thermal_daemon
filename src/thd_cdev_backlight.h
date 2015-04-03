/*
 * thd_cdev_backlight.h: thermal backlight cooling interface
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
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

#ifndef SRC_THD_CDEV_BACKLIGHT_H_
#define SRC_THD_CDEV_BACKLIGHT_H_

#include "thd_cdev.h"

class cthd_cdev_backlight: public cthd_cdev {
private:

public:
	cthd_cdev_backlight(unsigned int _index, int _cpu_index) :
			cthd_cdev(_index, "/sys/class/backlight/intel_backlight/") {
	}

	void set_curr_state(int state, int arg);
	int update();
};

#endif /* SRC_THD_CDEV_BACKLIGHT_H_ */
