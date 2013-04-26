/*
 * thd_cdev_pstates.cpp: thermal cooling class implementation
 *	using T states.
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
 */

#include <string>
#include <sstream>
#include <vector>
#include "thd_cdev_msr_pstates.h"
#include "thd_engine_dts.h"

/* Controls p states using direly MSRs. If there is no Intel P state driver
 * this acts as alternative.
 * When it takes control it will switch to "userspace" governor so that
 * it can control and restores later at the end.
 *
 */

int cthd_cdev_pstate_msr::init()
{
	thd_log_debug("pstate msr Index = %d CPU %d\n", index, cpu_index);
	// Get number of CPUs
	if(cdev_sysfs.exists("present"))
	{
		std::string count_str;
		size_t p0 = 0, p1;

		cdev_sysfs.read("present", count_str);
		p1 = count_str.find_first_of("-", p0);
		std::string token1 = count_str.substr(p0, p1 - p0);
		std::istringstream(token1) >> cpu_start_index;
		std::string token2 = count_str.substr(p1 + 1);
		std::istringstream(token2) >> cpu_end_index;
	}
	else
	{
		cpu_start_index = 0;
		cpu_end_index = 0;
		return THD_ERROR;
	}
	thd_log_debug("pstate msr CPU present %d-%d\n", cpu_start_index, cpu_end_index)
	;

	return THD_SUCCESS;
}

int cthd_cdev_pstate_msr::control_begin()
{
	std::string governor;

	if(cpu_index ==  - 1)
	{

		if(cdev_sysfs.exists("cpu0/cpufreq/scaling_governor"))
			cdev_sysfs.read("cpu0/cpufreq/scaling_governor", governor);

		if(governor == "userspace")
			return THD_SUCCESS;

		if(cdev_sysfs.exists("cpu0/cpufreq/scaling_governor"))
			cdev_sysfs.read("cpu0/cpufreq/scaling_governor", last_governor);

		for(int i = cpu_start_index; i <= cpu_end_index; ++i)
		{
			std::stringstream str;
			str << "cpu" << i << "/cpufreq/scaling_governor";
			if(cdev_sysfs.exists(str.str()))
			{
				cdev_sysfs.write(str.str(), "userspace");
			}
		}
	}
	else
	{
		std::stringstream str;
		str << "cpu" << cpu_index << "/cpufreq/scaling_governor";
		if(cdev_sysfs.exists(str.str()))
		{
			cdev_sysfs.read(str.str(), governor);
			if(governor == "userspace")
				return THD_SUCCESS;
			cdev_sysfs.read(str.str(), last_governor);

			cdev_sysfs.write(str.str(), "userspace");
		}

	}

	return THD_SUCCESS;
}

int cthd_cdev_pstate_msr::control_end()
{
	if(cpu_index ==  - 1)
	{
		for(int i = cpu_start_index; i <= cpu_end_index; ++i)
		{
			std::stringstream str;
			str << "cpu" << i << "/cpufreq/scaling_governor";
			if(cdev_sysfs.exists(str.str()))
			{
				cdev_sysfs.write(str.str(), last_governor);
			}
		}
	}
	else
	{
		std::stringstream str;
		str << "cpu" << cpu_index << "/cpufreq/scaling_governor";
		if(cdev_sysfs.exists(str.str()))
		{
			cdev_sysfs.write(str.str(), last_governor);
		}
	}

	return THD_SUCCESS;
}

void cthd_cdev_pstate_msr::set_curr_state(int state, int arg)
{
	if(state == 1)
	{
		thd_log_debug("CTRL begin.. cpu_index = %d\n", cpu_index);
		control_begin();
	}

	if(cpu_index ==  - 1)
	{
		int cpus = msr.get_no_cpus();
		for(int i = 0; i < cpus; ++i)
		{
			if(thd_engine->apply_cpu_operation(i) == false)
				continue;
			if(msr.set_freq_state_per_cpu(i, highest_freq_state - state) == THD_SUCCESS)
				curr_state = state;
			else
				curr_state = (state == 0) ? 0 : max_state;
		}
	}
	else
	{
		if(thd_engine->apply_cpu_operation(cpu_index))
		{
			if(msr.set_freq_state_per_cpu(cpu_index, highest_freq_state - state)  == THD_SUCCESS)
				curr_state = state;
			else
				curr_state = (state == 0) ? 0 : max_state;
		}
	}

	if(state == 0)
	{
		thd_log_debug("CTRL end..\n");
		control_end();
	}
}

int cthd_cdev_pstate_msr::get_max_state()
{

	return max_state;
}

int cthd_cdev_pstate_msr::update()
{
	highest_freq_state = msr.get_max_freq();
	if (highest_freq_state == THD_ERROR)
	{
		thd_log_warn("update: Read MSR failed \n");
		return THD_ERROR;
	}
	lowest_freq_state = msr.get_min_freq();
	if (lowest_freq_state == THD_ERROR)
	{
		thd_log_warn("update: Read MSR failed \n");
		return THD_ERROR;
	}
	thd_log_debug("cthd_cdev_pstate_msr min %x max %x\n", lowest_freq_state,
	highest_freq_state);
	max_state = highest_freq_state - lowest_freq_state;

	return init();
}
