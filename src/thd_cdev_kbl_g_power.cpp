/*
 * thd_cdev_kbl_amdgpu: amdgpu power control
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

#include "thd_cdev_kbl_g_power.h"
#include "thd_engine.h"

cthd_cdev_kbl_g_power::cthd_cdev_kbl_g_power(unsigned int _index,
		int _cpu_index) :
		cthd_cdev(_index, ""), activated(0) {
}

void cthd_cdev_kbl_g_power::set_curr_state(int state, int arg) {
	set_curr_state_raw(state, arg);
}

void cthd_cdev_kbl_g_power::set_curr_state_raw(int state, int arg) {
	thd_log_info("%s %d %d %d %d\n", __func__, state, arg,
			cdev_gpu->get_curr_state(0), cdev_cpu->get_curr_state(0));

	pb.add_new_sample(0, cdev_gpu->get_curr_state(0));
	pb.add_new_sample(1, cdev_cpu->get_curr_state(0));
	pb.power_balance();
	thd_log_info("%s id:0 %d \n", __func__, (int) pb.get_power(0));
	thd_log_info("%s id:1: %d \n", __func__, (int) pb.get_power(1));

	cdev_gpu->set_curr_state_raw(pb.get_power(0), 1);
	cdev_cpu->set_curr_state_raw(pb.get_power(1), 1);

	curr_state = state;
}

int cthd_cdev_kbl_g_power::thd_cdev_set_state(int set_point, int target_temp,
		int temperature, int hard_target, int state, int zone_id, int trip_id,
		int target_state_valid, int target_value, pid_param_t *pid_param,
		cthd_pid& pid, bool force) {

	time_t tm;
	int cpu_power, gpu_power;

	time(&tm);
	thd_log_info(
			">>cthd_cdev_kbl_g_power temperature %d:%d index:%d state:%d :zone:%d trip_id:%d target_state_valid:%d target_value :%d force:%d set_point:%d\n",
			target_temp, temperature, index, state, zone_id, trip_id,
			target_state_valid, target_value, force, set_point);

	thd_log_info("%s %d %d %d %d\n", __func__, state, 0,
			cdev_gpu->get_curr_state(0), cdev_cpu->get_curr_state(0));

	if (state) {
		pb.set_pid_target(set_point);
		pb.add_new_sample(0, cdev_gpu->get_curr_state(0));
		pb.add_new_sample(1, cdev_cpu->get_curr_state(0));
		pb.power_balance();
		gpu_power = pb.get_power(0);
		cpu_power = pb.get_power(1);
	} else {
		pb.reset();
		gpu_power = cdev_gpu->get_min_state();
		cpu_power = cdev_cpu->get_min_state();
	}

	thd_log_info("%s id:0 %d \n", __func__, gpu_power);
	thd_log_info("%s id:1: %d \n", __func__, cpu_power);

	/* thd_cdev_set_state() in the thd_cpu.cpp will not delete entry
	 * on state == 0 when the target value matched, which set state to 1.
	 * So here we are first causing the force == 1 to delet the previous
	 * target and then add a new target which is result of a power clamp
	 * when the current power is lower than the required cap.
	 */
	cdev_gpu->thd_cdev_set_state(set_point, target_temp, temperature,
			hard_target, state, zone_id, trip_id, 1, gpu_power, pid_param, pid,
			1);

	cdev_gpu->thd_cdev_set_state(set_point, target_temp, temperature,
			hard_target, state, zone_id, trip_id, 1, gpu_power, pid_param, pid, 0);

	cdev_cpu->thd_cdev_set_state(set_point, target_temp, temperature,
			hard_target, state, zone_id, trip_id, 1, cpu_power, pid_param, pid,
			1);

	cdev_cpu->thd_cdev_set_state(set_point, target_temp, temperature,
			hard_target, state, zone_id, trip_id, 1, cpu_power, pid_param, pid, 0);

	curr_state = state;

	return THD_SUCCESS;
}

int cthd_cdev_kbl_g_power::update() {

	cdev_gpu = thd_engine->search_cdev("amdgpu");
	if (!cdev_gpu) {
		thd_log_info("amdgpu failed\n");
		return THD_ERROR;
	}

	cdev_cpu = thd_engine->search_cdev("rapl_controller");
	if (!cdev_cpu) {
		thd_log_info("rapl_controller, failed\n");
		return THD_ERROR;
	}

	pb.add_new_power_domain("amd-gpu", 0.6, cdev_gpu->get_phy_max_state(),
			cdev_gpu->get_min_state());

	pb.add_new_power_domain("pkg-rapl", 0.4, cdev_cpu->get_phy_max_state(),
			cdev_cpu->get_min_state());

	pb.set_pid_param(-0.1, 0, 0);
	pb.set_pid_target(50000000);
	pb.dump_domains();

	max_state = cdev_gpu->get_phy_max_state() + cdev_cpu->get_phy_max_state();
	min_state = 0;

	return THD_SUCCESS;
}
