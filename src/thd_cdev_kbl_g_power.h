/*
 * thd_cdev_kbl_amdgpu: amdgpu power control
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

#ifndef SRC_THD_CDEV_KBL_G_POWER_H_
#define SRC_THD_CDEV_KBL_G_POWER_H_

#include "thd_cdev.h"
#include "power_balancer.h"

class cthd_cdev_kbl_g_power: public cthd_cdev {
private:
	int activated;
	power_balancer pb;
	cthd_cdev *cdev_gpu, *cdev_cpu;
public:
	cthd_cdev_kbl_g_power(unsigned int _index, int _cpu_index);
	void set_curr_state(int state, int arg);
	void set_curr_state_raw(int state, int arg);
	int update();
	int thd_cdev_set_state(int set_point, int target_temp, int temperature,
			int hard_target, int state, int zone_id, int trip_id,
			int target_state_valid, int target_value, pid_param_t *pid_param,
			cthd_pid& pid, bool force);
};

#endif /* SRC_THD_CDEV_BACKLIGHT_H_ */
