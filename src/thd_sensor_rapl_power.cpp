/*
 * thd_sensor_rapl_power.cpp: Power Sensor for RAPL
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
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

#include "thd_sensor_rapl_power.h"
#include "thd_engine.h"

cthd_sensor_rapl_power::cthd_sensor_rapl_power(int index) :
		cthd_sensor(index, "", "rapl_pkg_power", SENSOR_TYPE_RAW) {

	update_path("/sys/class/powercap/intel-rapl");
}

unsigned int cthd_sensor_rapl_power::read_temperature() {
	thd_engine->rapl_power_meter.rapl_start_measure_power();

	unsigned int pkg_power = thd_engine->rapl_power_meter.rapl_action_get_power(
			PACKAGE);

	pkg_power = (pkg_power / 1000);
	thd_log_debug("Sensor %s :power %u\n", type_str.c_str(), pkg_power);

	return pkg_power;
}
