/*
 * thd_zone_dts.cpp: thermal engine DTS class implementation
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * This implementation allows using core temperature interface.
 */

/* Implementation of DTS sensor Zone. This
 * - Identifies DTS sensors, and use maximum reported temperature to control
 * - Prioritize the cooling device order
 * - Sets one control point starting from target temperature (max) not critical.
 * Very first time it reaches this temperature cthd_model, calculates a set point when
 * cooling device needs to be activated.
 *
 */

#include <dirent.h>
#include "thd_zone_cpu.h"
#include "thd_engine_default.h"
#include "thd_cdev_order_parser.h"

const char *def_cooling_devices[] = { "rapl_controller", "intel_pstate",
		"intel_powerclamp", "cpufreq", "Processor", NULL };

cthd_zone_cpu::cthd_zone_cpu(int index, std::string path, int package_id) :
		cthd_zone(index, path, SENSORS_CORELATED), dts_sysfs(path.c_str()), critical_temp(
				0), max_temp(0), psv_temp(0), trip_point_cnt(0), sensor_mask(0), phy_package_id(
				package_id), pkg_thres_th_zone(-1), pkg_temp_poll_enable(false) {

	type_str = "cpu";
	thd_log_debug("zone dts syfs: %s, package id %d\n", path.c_str(),
			package_id);
}

int cthd_zone_cpu::init() {
	int temp = 0;
	bool found = false;

	max_temp = 0;
	critical_temp = 0;

	// Calculate the temperature trip points using the settings in coretemp
	for (int i = 0; i < max_dts_sensors; ++i) {
		std::stringstream temp_crit_str;
		std::stringstream temp_max_str;

		temp_crit_str << "temp" << i << "_crit";
		temp_max_str << "temp" << i << "_max";

		if (dts_sysfs.exists(temp_crit_str.str())) {
			std::string temp_str;
			dts_sysfs.read(temp_crit_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if (critical_temp == 0 || temp < critical_temp)
				critical_temp = temp;

		}

		if (dts_sysfs.exists(temp_max_str.str())) {

			// Set which index is present
			sensor_mask = sensor_mask | (1 << i);

			std::string temp_str;
			dts_sysfs.read(temp_max_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if (max_temp == 0 || temp < max_temp)
				max_temp = temp;
			found = true;
		}
	}
	if (!found) {
		thd_log_error("DTS temperature path not found\n");
		return THD_ERROR;
	}

	if (critical_temp == 0)
		critical_temp = def_critical_temp;

	if (max_temp == 0) {
		max_temp = critical_temp - def_offset_from_critical;
		thd_log_info("Force max temp to %d\n", max_temp);
	}

	if ((critical_temp - max_temp) < def_offset_from_critical) {
		max_temp = critical_temp - def_offset_from_critical;
		thd_log_info("Buggy max temp: to close to critical %d\n", max_temp);
	}

	// max_temperature is where the Fan would have been activated fully
	// psv_temp is set more so that in the case if Fan is not able to
	// control temperature, the passive temperature will be acted on
	psv_temp = max_temp + ((critical_temp - max_temp) / 2);

	thd_log_info("Core temp DTS :critical %d, max %d, psv %d\n", critical_temp,
			max_temp, psv_temp);

	return THD_SUCCESS;
}

int cthd_zone_cpu::load_cdev_xml(cthd_trip_point &trip_pt,
		std::vector<std::string> &list) {
	cthd_cdev *cdev;

	for (unsigned int i = 0; i < list.size(); ++i) {
		thd_log_debug("- %s\n", list[i].c_str());
		cdev = thd_engine->search_cdev(list[i]);
		if (cdev) {
			trip_pt.thd_trip_point_add_cdev(*cdev,
					cthd_trip_point::default_influence);
		}
	}

	return THD_SUCCESS;
}

int cthd_zone_cpu::parse_cdev_order() {
	cthd_cdev_order_parse parser;
	std::vector<std::string> order_list;
	int ret = THD_ERROR;

	if ((ret = parser.parser_init()) == THD_SUCCESS) {
		if ((ret = parser.start_parse()) == THD_SUCCESS) {
			ret = parser.get_order_list(order_list);
			if (ret == THD_SUCCESS) {
				cthd_trip_point trip_pt_passive(trip_point_cnt, PASSIVE,
						psv_temp, def_hystersis, index, DEFAULT_SENSOR_ID);
				trip_pt_passive.thd_trip_point_set_control_type(SEQUENTIAL);
				load_cdev_xml(trip_pt_passive, order_list);
				trip_points.push_back(trip_pt_passive);
				trip_point_cnt++;
			}
		}
		parser.parser_deinit();
		return ret;
	}

	return ret;
}

int cthd_zone_cpu::read_trip_points() {
	int ret;
	cthd_cdev *cdev;
	int i;

	ret = parse_cdev_order();
	if (ret == THD_SUCCESS) {
		thd_log_info("CDEVS order specified in thermal-cpu-cdev-order.xml\n");
		return THD_SUCCESS;
	}
	cthd_trip_point trip_pt_passive(trip_point_cnt, PASSIVE, psv_temp,
			def_hystersis, index, DEFAULT_SENSOR_ID);
	trip_pt_passive.thd_trip_point_set_control_type(SEQUENTIAL);
	i = 0;
	while (def_cooling_devices[i]) {
		cdev = thd_engine->search_cdev(def_cooling_devices[i]);
		if (cdev) {
			trip_pt_passive.thd_trip_point_add_cdev(*cdev,
					cthd_trip_point::default_influence);
		}
		++i;
	}
	trip_points.push_back(trip_pt_passive);
	trip_point_cnt++;

	// Add active trip point at the end
	cthd_trip_point trip_pt_active(trip_point_cnt, ACTIVE, max_temp,
			def_hystersis, index, DEFAULT_SENSOR_ID);
	trip_pt_active.thd_trip_point_set_control_type(SEQUENTIAL);

	cdev = thd_engine->search_cdev("Fan");
	if (cdev) {
		trip_pt_active.thd_trip_point_add_cdev(*cdev,
				cthd_trip_point::default_influence);
		trip_points.push_back(trip_pt_active);
		trip_point_cnt++;
	}

	return THD_SUCCESS;
}

int cthd_zone_cpu::zone_bind_sensors() {

	cthd_sensor *sensor;
	int status = THD_ERROR;
	bool async_sensor = false;

	if (init() != THD_SUCCESS)
		return THD_ERROR;

	if (!thd_engine->rt_kernel_status()) {
		sensor = thd_engine->search_sensor("pkg-temp-0");
		if (sensor) {
			bind_sensor(sensor);
			async_sensor = true;
		}
	}

	if (!thd_engine->rt_kernel_status()) {
		sensor = thd_engine->search_sensor("x86_pkg_temp");
		if (sensor) {
			bind_sensor(sensor);
			async_sensor = true;
		}
	}

	sensor = thd_engine->search_sensor("soc_dts0");
	if (sensor) {
		bind_sensor(sensor);
		async_sensor = true;
	}

	if (async_sensor) {
		sensor = thd_engine->search_sensor("hwmon");
		if (sensor) {
			sensor->set_async_capable(true);
		}
		return THD_SUCCESS;
	}

	// No package temp sensor fallback to core temp
	int cnt = 0;
	unsigned int mask = 0x1;
	do {
		if (sensor_mask & mask) {
			std::stringstream temp_input_str;
			temp_input_str << "temp" << cnt << "_input";
			cthd_sensor *sensor;

			sensor = thd_engine->search_sensor(temp_input_str.str());
			if (sensor) {
				bind_sensor(sensor);
				status = THD_SUCCESS;
			}
		}
		mask = (mask << 1);
		cnt++;
	} while (mask != 0);

	if (status != THD_SUCCESS) {
		thd_log_info("Trying to bind hwmon sensor\n");
		sensor = thd_engine->search_sensor("hwmon");
		if (sensor) {
			bind_sensor(sensor);
			status = THD_SUCCESS;
			thd_log_info("Bind hwmon sensor\n");
		}
	}

	return status;
}

int cthd_zone_cpu::read_cdev_trip_points() {
	return THD_SUCCESS;
}
