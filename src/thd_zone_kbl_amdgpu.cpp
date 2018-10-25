/*
 * thd_zone_kbl_amdgpu.cpp: thermal zone for KBL-G amdgpu
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

#include "thd_engine_default.h"
#include "thd_sys_fs.h"
#include "thd_zone_kbl_amdgpu.h"

cthd_zone_kbl_amdgpu::cthd_zone_kbl_amdgpu(int index): cthd_zone(index, ""), sensor(NULL), dts_sysfs("/sys/class/hwmon/"){
	type_str = "amdgpu";
}

int cthd_zone_kbl_amdgpu::zone_bind_sensors() {

	cthd_sensor *sensor;


	sensor = thd_engine->search_sensor("amdgpu-temperature");
	if (sensor) {
		bind_sensor(sensor);
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_zone_kbl_amdgpu::read_trip_points() {
	cthd_cdev *cdev;
	DIR *dir;

	struct dirent *entry;
	const std::string base_path = "/sys/class/hwmon/";
	int crit_temp = 0;

	thd_log_info("cthd_zone_kbl_amdgpu::read_trip_points \n");

	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			// Check name
			std::string name_path = base_path + entry->d_name + "/name";

			std::ifstream ifs(name_path.c_str(), std::ifstream::in);
			if (ifs.good()) {
				std::string line;
				while (std::getline(ifs, line)) {
					if (line == "amdgpu") {
						std::string path = base_path + entry->d_name + "/";
						csys_fs hwmon_sysfs(path.c_str());

						if (!hwmon_sysfs.exists("temp1_crit")) {
							thd_log_info("path failed %s\n", path.c_str());
							closedir(dir);
							return THD_ERROR;
						}
						int ret = hwmon_sysfs.read("temp1_crit", &crit_temp);
						if (ret < 0) {
							thd_log_info("crit temp failed\n");
							closedir(dir);
							return THD_ERROR;
						}
					}
				}
				ifs.close();
			}
		}
		closedir(dir);
	}

	if (!crit_temp)
		return THD_ERROR;

	cdev = thd_engine->search_cdev("amdgpu");
	if (!cdev) {
		thd_log_info("amdgpu failed\n");

		return THD_ERROR;
	}

	cthd_trip_point trip_pt_passive(0, PASSIVE, crit_temp - 10000,
					0, index, DEFAULT_SENSOR_ID);

	trip_pt_passive.thd_trip_point_add_cdev(*cdev,
						cthd_trip_point::default_influence);

	trip_points.push_back(trip_pt_passive);

	cthd_trip_point trip_pt_critical(1, CRITICAL, crit_temp,
					0, index, DEFAULT_SENSOR_ID);
	trip_pt_passive.thd_trip_point_add_cdev(*cdev,
						cthd_trip_point::default_influence);

	trip_points.push_back(trip_pt_critical);

	thd_log_info("cthd_zone_kbl_amdgpu::read_trip_points  OK\n");

	return THD_SUCCESS;
}


int cthd_zone_kbl_amdgpu::read_cdev_trip_points() {
	return THD_SUCCESS;
}
