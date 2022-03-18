/*
 * thd_int3400.cpp: Load and check INT3400 uuids for match
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

#include "thd_int3400.h"
#include "thd_cdev_gen_sysfs.h"
#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>

cthd_INT3400::cthd_INT3400(std::string _uuid) : uuid(_uuid), base_path("") {
	csys_fs cdev_sysfs("");

	if (cdev_sysfs.exists("/sys/bus/acpi/devices/INT3400:00/physical_node/uuids")) {
		base_path = "/sys/bus/acpi/devices/INT3400:00/physical_node/uuids/";
	} else if (cdev_sysfs.exists("/sys/bus/acpi/devices/INTC1040:00/physical_node/uuids")) {
		base_path = "/sys/bus/acpi/devices/INTC1040:00/physical_node/uuids/";
	} else if (cdev_sysfs.exists("/sys/bus/acpi/devices/INTC1041:00/physical_node/uuids")) {
		base_path = "/sys/bus/acpi/devices/INTC1041:00/physical_node/uuids/";
	}
	thd_log_info("INT3400 Base path is %s\n", base_path.c_str());
}

int cthd_INT3400::match_supported_uuid() {
	if (base_path == "")
		return THD_ERROR;

	std::string filename = base_path + "available_uuids";

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (ifs.good()) {
		std::string line;
		while (std::getline(ifs, line)) {
			thd_log_debug("uuid: %s\n", line.c_str());
			if (line == uuid)
				return THD_SUCCESS;
		}
		ifs.close();
	}

	return THD_ERROR;
}

void cthd_INT3400::set_default_uuid(void) {
	if (set_policy_osc() == THD_SUCCESS)
		return;

	if (base_path == "")
		return;

	std::string filename = base_path + "current_uuid";

	std::ofstream ofs(filename.c_str(), std::ofstream::out);
	if (ofs.good()) {
		thd_log_info("Set Default UUID: %s\n", uuid.c_str());
		ofs << uuid;
	}
}

int cthd_INT3400::set_policy_osc(void) {
	if (base_path == "")
		return THD_ERROR;

	std::string filename = base_path + "available_uuids";

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (ifs.good()) {
		std::string line;
		if (std::getline(ifs, line)) {
			thd_log_debug("available uuids: %s\n", line.c_str());
			if (line == "UNKNOWN") {
				std::string _filename = base_path + "current_uuid";

				std::ofstream ofs(_filename.c_str(), std::ofstream::out);
				if (ofs.good()) {
					std::string _uuid = "42A441D6-AE6A-462b-A84B-4A8CE79027D3";

					thd_log_info("Set Default UUID: %s\n", _uuid.c_str());
					ofs << _uuid;
				}

				return THD_SUCCESS;
			}
		}
		ifs.close();
	}

	return THD_ERROR;
}
