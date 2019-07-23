/*
 * thd_sensor_kbl_amdgpu_power.cpp: Power Sensor for KBL-G amdgpu
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

#include "thd_sensor_kbl_g_mcp.h"
#include "thd_engine.h"

cthd_sensor_kbl_g_mcp::cthd_sensor_kbl_g_mcp(int index) :
		cthd_sensor(index, "", "kbl-g-mcp", SENSOR_TYPE_RAW) {

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
						update_path(
								base_path + entry->d_name + "/power1_average");
						set_scale(1000);
					}
				}
				ifs.close();
			}
		}
		closedir(dir);
	}
}

unsigned int cthd_sensor_kbl_g_mcp::read_temperature()
{
	csys_fs sysfs;
	std::string buffer;
	int gpu_power;
	int ret;

	thd_engine->rapl_power_meter.rapl_start_measure_power();

	ret = sensor_sysfs.read("", &gpu_power);
	if (ret <= 0)
		gpu_power = 0;

	unsigned int pkg_power = thd_engine->rapl_power_meter.rapl_action_get_power(
					PACKAGE);
	thd_log_debug("Sensor %s :temp %u %u total %u \n", type_str.c_str(),
			gpu_power, pkg_power, gpu_power + pkg_power);

	return (unsigned int)(gpu_power + pkg_power);
}
