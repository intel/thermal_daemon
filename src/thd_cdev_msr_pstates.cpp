/*
 * thd_cdev_pstates.cpp: thermal cooling class implementation
 *	using T states.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

#include <string>
#include <sstream>
#include <vector>
#include "thd_cdev_msr_pstates.h"
#include "thd_engine_dts.h"


int cthd_cdev_pstate_msr::init()
{
	thd_log_debug("pstate msr CPU present \n");
	// Get number of CPUs
	if (cdev_sysfs.exists("present")) {
		std::string count_str;
		size_t p0 = 0, p1;

		cdev_sysfs.read("present", count_str);
		p1 = count_str.find_first_of("-", p0);
		std::string token1 = count_str.substr(p0, p1 - p0);
		std::istringstream(token1) >> cpu_start_index;
		std::string token2 = count_str.substr(p1+1);
		std::istringstream(token2) >> cpu_end_index;
	} else {
		cpu_start_index = 0;
		cpu_end_index = 0;
		return THD_ERROR;
	}
	thd_log_debug("pstate msr CPU present %d-%d\n", cpu_start_index, cpu_end_index);

	return THD_SUCCESS;
}

int cthd_cdev_pstate_msr::control_begin()
{
	std::string governor;

	if (cpu_index == -1) {

		if (cdev_sysfs.exists("cpu0/cpufreq/scaling_governor"))
			cdev_sysfs.read("cpu0/cpufreq/scaling_governor", governor);

		if (governor == "userspace")
			return THD_SUCCESS;

		if (cdev_sysfs.exists("cpu0/cpufreq/scaling_governor"))
			cdev_sysfs.read("cpu0/cpufreq/scaling_governor", last_governor);

		for (int i=cpu_start_index; i<=cpu_end_index; ++i) {
			std::stringstream str;
			str << "cpu" << i << "/cpufreq/scaling_governor";
			if (cdev_sysfs.exists(str.str())) {
				cdev_sysfs.write(str.str(), "userspace");
			}
		}
	} else {
		std::stringstream str;
		str << "cpu" << cpu_index << "/cpufreq/scaling_governor";
		if (cdev_sysfs.exists(str.str())) {
			cdev_sysfs.read(str.str(), governor);
			if (governor == "userspace")
				return THD_SUCCESS;
			cdev_sysfs.read(str.str(), last_governor);

			cdev_sysfs.write(str.str(), "userspace");
		}

	}

	return THD_SUCCESS;
}

int cthd_cdev_pstate_msr::control_end()
{
	if (cpu_index == -1) {
		for (int i=cpu_start_index; i<=cpu_end_index; ++i) {
			std::stringstream str;
			str << "cpu" << i << "/cpufreq/scaling_governor";
			if (cdev_sysfs.exists(str.str())) {
				cdev_sysfs.write(str.str(), last_governor);
			}
		}
	} else {
		std::stringstream str;
		str << "cpu" << cpu_index << "/cpufreq/scaling_governor";
		if (cdev_sysfs.exists(str.str())) {
			cdev_sysfs.write(str.str(), last_governor);
		}
	}

	return THD_SUCCESS;
}

void cthd_cdev_pstate_msr::set_curr_state(int state, int arg)
{
	int inc = 1;

	if (state == p_state_index)
		return;

	if (state > p_state_index )
		inc = 0;

	if (state == 1) {
		thd_log_debug("CTRL begin..\n");
		control_begin();
	}
	if (state == 1) {
		// Set to highest non turbo frequency
		if (cpu_index == -1) {
			int cpus = msr.get_no_cpus();
			for (int i=0; i<cpus; ++i) {
				if (thd_engine->apply_cpu_operation(i) == false)
					continue;
				msr.set_freq_state_per_cpu(i, highest_freq_state);
				curr_state = state;
				p_state_index = state;
			}
		}
		else {
			if (thd_engine->apply_cpu_operation(cpu_index)) {
				msr.set_freq_state_per_cpu(cpu_index, highest_freq_state);
				curr_state = state;
				p_state_index = state;
			}
		}
	} else if (inc && curr_state != 1) {
		if (cpu_index == -1) {
			int cpus = msr.get_no_cpus();
			for (int i=0; i<cpus; ++i) {
				if (thd_engine->apply_cpu_operation(i) == false)
					continue;
				msr.inc_freq_state_per_cpu(i);
				curr_state = state;
				p_state_index = state;
			}
		}
		else {
			if (thd_engine->apply_cpu_operation(cpu_index)) {
				msr.inc_freq_state_per_cpu(cpu_index);
				curr_state = state;
				p_state_index = state;
			}
		}
	} else if (inc == 0){
		if (cpu_index == -1) {
			int cpus = msr.get_no_cpus();
			for (int i=0; i<cpus; ++i) {
				if (thd_engine->apply_cpu_operation(i) == false)
					continue;
				msr.dec_freq_state_per_cpu(i);
				curr_state = state;
				p_state_index = state;
			}
		}
		else {
			if (thd_engine->apply_cpu_operation(cpu_index)) {
				msr.dec_freq_state_per_cpu(cpu_index);
				curr_state = state;
				p_state_index = state;
			}
		}
	} else {
		curr_state = state;
		p_state_index = state;
	}
	if (state == 0) {
		thd_log_debug("CTRL end..\n");
		control_end();
	}
}

int cthd_cdev_pstate_msr::get_max_state()
{
	highest_freq_state = msr.get_max_freq();
	lowest_freq_state = msr.get_min_freq();

	return (highest_freq_state - lowest_freq_state) ;
}

int cthd_cdev_pstate_msr::update()
{
	return init();
}
