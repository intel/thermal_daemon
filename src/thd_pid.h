/*
 * thd_pid.h: pid interface
 *
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

#include "thermald.h"
#include <time.h>

class cthd_pid {

private:
	double err_sum, last_err;
	time_t last_time;
	unsigned int target_temp;

public:
	cthd_pid();
	double kp, ki, kd;
	int pid_output(unsigned int curr_temp);
	void set_target_temp(unsigned int temp) {
		target_temp = temp;
	}
	void reset() {
		err_sum = last_err = last_time = 0;
	}
};
