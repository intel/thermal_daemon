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
#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>

cthd_INT3400::cthd_INT3400(std::string _uuid) : uuid(_uuid) {
}

int cthd_INT3400::match_supported_uuid() {
	std::string filename =
			"/sys/bus/acpi/devices/INT3400:00/physical_node/uuids/available_uuids";

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (ifs.good()) {
		std::string line;
		while (std::getline(ifs, line)) {
			thd_log_debug("uuid: %s\n", line.c_str());
			if (line == uuid || line == "UNKNOWN")
				return THD_SUCCESS;
		}
		ifs.close();
	}

	return THD_ERROR;
}

void cthd_INT3400::set_default_uuid(void) {
	std::string filename =
			"/sys/bus/acpi/devices/INT3400:00/physical_node/uuids/current_uuid";

	std::ofstream ofs(filename.c_str(), std::ofstream::out);
	if (ofs.good()) {
		thd_log_info("Set Default UUID: %s\n", uuid.c_str());
		ofs << uuid;
	}
}
