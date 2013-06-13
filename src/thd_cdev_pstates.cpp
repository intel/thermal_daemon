/*
 * thd_cdev_pstates.cpp: thermal cooling class implementation
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
 */

/* Control P states using cpufreq. Each step reduces to next lower frequency
 *
 */

#include "thd_cdev_pstates.h"
#include "thd_engine_dts.h"

int cthd_cdev_pstates::init()
{
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
	thd_log_debug("pstate CPU present %d-%d\n", cpu_start_index, cpu_end_index);

	// Get list of available frequencies for each CPU
	// Assuming every core supports same sets of frequencies, so
	// just reading for cpu0
	std::vector < std::string > _cpufreqs;
	if(cdev_sysfs.exists("cpu0/cpufreq/scaling_available_frequencies"))
	{
		std::string p = 
	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies";

		std::ifstream f(p.c_str(), std::fstream::in);
		if(f.fail())
			return  - EINVAL;

		while(!f.eof())
		{
			std::string token;

			f >> token;
			if(!f.bad())
			{
				if(!token.empty())
					_cpufreqs.push_back(token);
			}
		}
		f.close();
	}

	// Check scaling max frequency and min frequency
	// Remove frequencies above and below this in the freq list
	// The available list contains these frequencies even if they are not allowed
	unsigned int scaling_min_frqeuency = 0;
	unsigned int scaling_max_frqeuency = 0;
	for(int i = cpu_start_index; i <= cpu_end_index; ++i)
	{
		std::stringstream str;
		std::string freq_str;
		str << "cpu" << i << "/cpufreq/scaling_min_freq";
		if(cdev_sysfs.exists(str.str()))
		{
			cdev_sysfs.read(str.str(), freq_str);
			unsigned int freq_int;
			std::istringstream(freq_str) >> freq_int;
			if(scaling_min_frqeuency == 0 || freq_int < scaling_min_frqeuency)
				scaling_min_frqeuency = freq_int;
		}
	}

	for(int i = cpu_start_index; i <= cpu_end_index; ++i)
	{
		std::stringstream str;
		std::string freq_str;
		str << "cpu" << i << "/cpufreq/scaling_max_freq";
		if(cdev_sysfs.exists(str.str()))
		{
			cdev_sysfs.read(str.str(), freq_str);

			unsigned int freq_int;
			std::istringstream(freq_str) >> freq_int;
			if(scaling_max_frqeuency == 0 || freq_int > scaling_max_frqeuency)
				scaling_max_frqeuency = freq_int;
		}
	}

	thd_log_debug("cpu freq max %u min %u\n", scaling_max_frqeuency,
	scaling_min_frqeuency);

	for(unsigned int i = 0; i < _cpufreqs.size(); ++i)
	{
		thd_log_debug("cpu freq Add %d: %s\n", i, _cpufreqs[i].c_str());

		unsigned int freq_int;
		std::istringstream(_cpufreqs[i]) >> freq_int;

		if(freq_int >= scaling_min_frqeuency && freq_int <= scaling_max_frqeuency)
		{
			cpufreqs.push_back(freq_int);
		}
	}

	for(unsigned int i = 0; i < cpufreqs.size(); ++i)
	{
		thd_log_debug("cpu freq %d: %d\n", i, cpufreqs[i]);
	}

	pstate_active_freq_index = 0;

	return THD_SUCCESS;
}

void cthd_cdev_pstates::set_curr_state(int state, int arg)
{
	// p state control
	//scaling_setspeed
	if(state < (int)cpufreqs.size())
	{
		if(cpu_index ==  - 1)
		{
			for(int i = cpu_start_index; i <= cpu_end_index; ++i)
			{
				if(thd_engine->apply_cpu_operation(i) == false)
					continue;
				std::stringstream str;
				str << "cpu" << i << "/cpufreq/scaling_max_freq";
				if(cdev_sysfs.exists(str.str()))
				{
					std::stringstream speed;
					speed << cpufreqs[state];
					cdev_sysfs.write(str.str(), speed.str());
				}
				pstate_active_freq_index = state;
				curr_state = state;
			}
		}
		else
		{
			if(thd_engine->apply_cpu_operation(cpu_index))
			{
				std::stringstream str;
				str << "cpu" << cpu_index << "/cpufreq/scaling_max_freq";
				if(cdev_sysfs.exists(str.str()))
				{
					std::stringstream speed;
					speed << cpufreqs[state];
					cdev_sysfs.write(str.str(), speed.str());
				}
				pstate_active_freq_index = state;
				curr_state = state;
			}
		}
	}

}

int cthd_cdev_pstates::get_max_state()
{
	return cpufreqs.size();
}

int cthd_cdev_pstates::update()
{
	return init();
}
