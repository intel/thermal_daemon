/*
 * thd_sensor.h: thermal sensor class implementation
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "thd_sensor.h"
#include "thd_engine.h"

cthd_sensor::cthd_sensor(int _index, std::string control_path,
		std::string _type_str, int _type) :
		index(_index), type(_type), sensor_sysfs(control_path.c_str()), sensor_active(
				false), type_str(std::move(_type_str)), async_capable(false), virtual_sensor(
				false), thresholds(0), scale(1) {

}

int cthd_sensor::sensor_update() {
	if (type == SENSOR_TYPE_THERMAL_SYSFS) {
		if (sensor_sysfs.exists("type")) {
			sensor_sysfs.read("type", type_str);
			thd_log_info("sensor_update: type %s\n", type_str.c_str());
		} else
			return THD_ERROR;

		if (sensor_sysfs.exists("temp")) {
			return THD_SUCCESS;
		} else {
			thd_log_msg("sensor id %d: No temp sysfs for reading temp\n",
					index);
			return THD_ERROR;
		}
	}
	if (type == SENSOR_TYPE_RAW) {
		if (sensor_sysfs.exists("")) {
			return THD_SUCCESS;
		} else {
			thd_log_msg("sensor id %d %s: No temp sysfs for reading raw temp\n",
					index, sensor_sysfs.get_base_path());
			return THD_ERROR;
		}
	}
	return THD_SUCCESS;
}

unsigned int cthd_sensor::read_temperature() {
	csys_fs sysfs;
	std::string buffer;
	int temp;

	thd_log_debug("read_temperature sensor ID %d\n", index);
	if (type == SENSOR_TYPE_THERMAL_SYSFS)
		sensor_sysfs.read("temp", buffer);
	else
		sensor_sysfs.read("", buffer);
	std::istringstream(buffer) >> temp;
	if (temp < 0)
		temp = 0;
	thd_log_debug("Sensor %s :temp %u\n", type_str.c_str(), temp);
	return (unsigned int)temp / scale;
}

void cthd_sensor::enable_uevent() {
	csys_fs cdev_sysfs("/sys/class/thermal/");
	std::stringstream policy_sysfs;

	policy_sysfs << "thermal_zone" << index << "/policy";
	if (cdev_sysfs.exists(policy_sysfs.str().c_str())) {
		cdev_sysfs.write(policy_sysfs.str(), "user_space");
	}
}

int cthd_sensor::set_threshold(int index, int temp) {
	if (type != SENSOR_TYPE_THERMAL_SYSFS)
		return THD_ERROR;

	std::stringstream tcdev;
	std::stringstream thres;
	int status = 0;

	if (thd_engine->get_poll_interval())
		return THD_SUCCESS;

	if (!async_capable) {
		return THD_ERROR;
	}
	tcdev << "trip_point_" << index << "_temp";
	thres << temp;
	if (sensor_sysfs.exists(tcdev.str().c_str())) {
		status = sensor_sysfs.write(tcdev.str(), thres.str());
	}
	thd_log_debug("cthd_sensor::set_threshold: status %d\n", status);

	if (status > 0) {
		enable_uevent();
		return THD_SUCCESS;
	} else
		return THD_ERROR;

}

void cthd_sensor::sensor_poll_trip(bool status) {
	if (status)
		thd_engine->thd_engine_poll_enable(index);
	else
		thd_engine->thd_engine_poll_disable(index);
}

void cthd_sensor::sensor_fast_poll(bool status) {
	if (status)
		thd_engine->thd_engine_fast_poll_enable(index);
	else
		thd_engine->thd_engine_fast_poll_disable(index);
}
