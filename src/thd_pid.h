/*
 * thd_pid.h: PID controller
 *
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

#ifndef THD_PID_H
#define THD_PID_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include <vector>

class cthd_pid {

private:
	csys_fs 		cdev_sysfs;
	int		 		last_time;
	float 			err_sum, last_err;
	float 			kp, ki, kd;
	std::vector <int> 	cdev_indexes;

	pthread_attr_t crazy_thread_attr;
	pthread_t 	crazy_running_thread;
	bool		calibrated;
	std::string	sensor_path;

	int 		getMilliCount();

public:
	cthd_pid();
	//cthd_pid(float Kp, float Ki, float Kd);
	void set_pid_params(float Kp, float Ki, float Kd);
	void calibrate();
	int update_pid(int temp, int setpoint);
	void 		close_loop_tune();

	void add_sensor_path(std::string path);
	void add_cdev_index(int index);

	void print_pid_constants();
};

#endif
