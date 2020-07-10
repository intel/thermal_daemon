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

#include <dirent.h>

#include "thd_cdev_kbl_amdgpu.h"

cthd_cdev_kgl_amdgpu::cthd_cdev_kgl_amdgpu(unsigned int _index, int _cpu_index) :
		cthd_cdev(_index, ""),activated(0) {
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/hwmon/";

	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			// Check name
			std::string name_path = base_path + entry->d_name + "/name";

			std::ifstream ifs(name_path.c_str(), std::ifstream::in);
			if (ifs.good()) {
				std::string line;
				while (std::getline(ifs, line)) {
					if (line == "amdgpu") {
						cdev_sysfs.update_path(base_path + entry->d_name + "/");
					}
				}
				ifs.close();
			}
		}
		closedir(dir);
	}
}

int cthd_cdev_kgl_amdgpu::get_curr_state() {
	if (activated)
		return get_curr_state(true);
	else
		return min_state;
}

int cthd_cdev_kgl_amdgpu::get_curr_state(bool read_again) {
	int ret;
	int state;

	ret = cdev_sysfs.read("power1_average", &state);
	if (ret < 0) {
		return min_state;
	}

	//workaround: Looks for comment in set_curr_state
	if (state == 1000000)
		state = max_state;

	return state;
}

void cthd_cdev_kgl_amdgpu::set_curr_state(int state, int arg) {
	int new_state = state;

	if (!arg) {
		activated = 0;
		thd_log_info("ignore \n");
		new_state = min_state;
	} else
		activated = 1;

	// When power1_cap = 0 is written to sysfs it changes to max powercap value
	// So this is a workaround
	if (state == 0)
		new_state = min_state;

	if (cdev_sysfs.write("power1_cap", new_state) > 0)
		curr_state = state;

	thd_log_info("set cdev state index %d state %d wr:%d\n", index, state, state);
}

void cthd_cdev_kgl_amdgpu::set_curr_state_raw(int state, int arg) {
	set_curr_state(state, arg);
}

int cthd_cdev_kgl_amdgpu::update() {
	// min and max are opposite of actual sysfs value in thermald terms
	if (cdev_sysfs.exists("power1_cap_min")) {
		std::string type_str;
		int ret;

		ret = cdev_sysfs.read("power1_cap_min", &max_state);
		if (ret < 0) {
			thd_log_info("cthd_cdev_kgl_amdgpu : not present\n");
			return THD_ERROR;
		}
	} else {
		return THD_ERROR;
	}

	if (cdev_sysfs.exists("power1_cap_max")) {
		std::string type_str;
		int ret;

		ret = cdev_sysfs.read("power1_cap_max", &min_state);
		if (ret < 0) {
			thd_log_info("cthd_cdev_kgl_amdgpu : not present\n");
			return THD_ERROR;
		}
	} else {
		return THD_ERROR;
	}

	set_inc_dec_value(-(min_state * (float) 10 / 100));
        set_pid_param(-0.4, 0, 0);

	return THD_SUCCESS;
}

int cthd_cdev_kgl_amdgpu::map_target_state(int target_valid, int target_state) {
	return 0;
}

int cthd_cdev_kgl_amdgpu::get_phy_max_state() {

        return min_state;
}
