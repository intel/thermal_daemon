/*
 * cthd_cdev_rapl_dram.cpp: thermal cooling class implementation
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

#include <dirent.h>
#include "thd_cdev_rapl_dram.h"
#include "thd_engine.h"

int cthd_sysfs_cdev_rapl_dram::update() {
	DIR *dir;
	struct dirent *entry;
	std::string base = "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/";
	bool found = false;
	std::string path_name;

	dir = opendir(base.c_str());
	if (!dir)
		return THD_ERROR;

	while ((entry = readdir(dir)) != NULL) {
		std::string temp_str;

		temp_str = base + entry->d_name + "/" + "name";
		csys_fs name_sysfs(temp_str.c_str());
		if (!name_sysfs.exists()) {
			continue;
		}
		std::string name;
		if (name_sysfs.read("", name) < 0) {
			continue;
		}
		thd_log_info("name = %s\n", name.c_str());
		if (name == "dram") {
			found = true;
			path_name = base + entry->d_name + "/";
			break;
		}
	}

	closedir(dir);

	if (!found)
		return THD_ERROR;

	cdev_sysfs.update_path(path_name);

	return cthd_sysfs_cdev_rapl::update();
}

bool cthd_sysfs_cdev_rapl_dram::calculate_phy_max() {
	if (dynamic_phy_max_enable) {
		unsigned int curr_max_phy;
		curr_max_phy = thd_engine->rapl_power_meter.rapl_action_get_power(DRAM);
		thd_log_info("curr_phy_max = %u \n", curr_max_phy);
		if (curr_max_phy < rapl_min_default_step)
			return false;
		if (phy_max < curr_max_phy) {
			phy_max = curr_max_phy;
			set_inc_dec_value(phy_max * (float) rapl_power_dec_percent / 100);
			max_state = phy_max;
			max_state -= (float) max_state * rapl_low_limit_percent / 100;
			thd_log_info("DRAM PHY_MAX %lu, step %d, max_state %d\n", phy_max,
					inc_dec_val, max_state);
		}
	}

	return true;
}
