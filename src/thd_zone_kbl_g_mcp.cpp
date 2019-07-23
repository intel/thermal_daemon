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
#include "thd_zone_kbl_g_mcp.h"

cthd_zone_kbl_g_mcp::cthd_zone_kbl_g_mcp(int index): cthd_zone(index, ""), sensor(NULL), dts_sysfs("/sys/class/hwmon/"){
	type_str = "kbl-g-mcp";
}

int cthd_zone_kbl_g_mcp::zone_bind_sensors() {

	cthd_sensor *sensor;


	sensor = thd_engine->search_sensor("kbl-g-mcp");
	if (sensor) {
		bind_sensor(sensor);
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_zone_kbl_g_mcp::read_trip_points() {
	cthd_cdev *cdev_gpu, *cdev_cpu;

	thd_log_info("cthd_zone_kbl_g_mcp::read_trip_points \n");

	cdev_gpu = thd_engine->search_cdev("amdgpu");
	if (!cdev_gpu) {
		thd_log_info("amdgpu failed\n");

		return THD_ERROR;
	}

	int gpu_max_power = cdev_gpu->get_phy_max_state();

	cdev_cpu = thd_engine->search_cdev("rapl_controller");
	if (!cdev_cpu) {
		thd_log_info("rapl_controller, failed\n");

		return THD_ERROR;
	}

	int cpu_max_power = cdev_cpu->get_phy_max_state();

	thd_log_debug("gpu %d cpu %d\n", gpu_max_power, cpu_max_power);
	int total_power = gpu_max_power + cpu_max_power;

	cthd_trip_point trip_pt_passive(0, PASSIVE, total_power ,
					0, index, DEFAULT_SENSOR_ID, PARALLEL);

	pid_param_t pid_param;
	pid_param.valid = 1;
	pid_param.kp = -0.4f;
	pid_param.ki = 0.0f;
	pid_param.kd = 0.0f;
	thd_log_info("==pid valid %f:%f:%f\n", pid_param.kp, pid_param.ki, pid_param.kd);
	trip_pt_passive.thd_trip_point_add_cdev(*cdev_gpu,
			cthd_trip_point::default_influence, thd_engine->get_poll_interval(),
			0, 0, &pid_param);

	pid_param.kp = -0.6f;
	pid_param.ki = 0.0f;
	pid_param.kd = 0.0f;

	trip_pt_passive.thd_trip_point_add_cdev(*cdev_cpu,
			cthd_trip_point::default_influence, thd_engine->get_poll_interval(),
			0, 0, &pid_param);

	trip_points.push_back(trip_pt_passive);

	thd_log_info("cthd_zone_kbl_g_mcp::read_trip_points  OK\n");

	return THD_SUCCESS;
}


int cthd_zone_kbl_g_mcp::read_cdev_trip_points() {
	return THD_SUCCESS;
}
