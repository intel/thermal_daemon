/*
 * thd_sensor_kbl_amdgpu_thermal.cpp: Thermal Sensor for KBL-G amdgpu
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

#include "thd_sensor_kbl_amdgpu_thermal.h"

cthd_sensor_kbl_amdgpu_thermal::cthd_sensor_kbl_amdgpu_thermal(int index) :
		cthd_sensor(index, "", "amdgpu-temperature", SENSOR_TYPE_RAW) {

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
						update_path(base_path + entry->d_name + "/temp1_input");
					}
				}
				ifs.close();
			}
		}
		closedir(dir);
	}
}
