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

#ifndef SRC_THD_CDEV_KBL_AMD_GPU_H_
#define SRC_THD_CDEV_KBL_AMD_GPU_H_

#include "thd_cdev.h"

class cthd_cdev_kgl_amdgpu: public cthd_cdev {
private:
	int activated;
public:
	cthd_cdev_kgl_amdgpu(unsigned int _index, int _cpu_index);
	void set_curr_state(int state, int arg);
	void set_curr_state_raw(int state, int arg);
	int get_curr_state();
	int get_curr_state(bool read_again);
	int update();
	int map_target_state(int target_valid, int target_state);
	int get_phy_max_state();
};

#endif /* SRC_THD_CDEV_BACKLIGHT_H_ */
