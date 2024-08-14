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

	cdev_sysfs.update_path(std::move(path_name));

	return cthd_sysfs_cdev_rapl::update();
}

