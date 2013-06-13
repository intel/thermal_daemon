/*
 * thd_engine_dts.h: thermal engine dts class interface
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

#ifndef THD_ENGINE_DTS_H_
#define THD_ENGINE_DTS_H_


#include "thd_engine.h"
#include <time.h>

class cthd_engine_dts: public cthd_engine
{
private:
	csys_fs thd_sysfs;
	unsigned int cpu_mask_remaining;

public:
	static const unsigned int def_cpu_mask = 0xffff;
	unsigned int sensor_mask;

	cthd_engine_dts(): thd_sysfs("/sys/devices/platform/coretemp.0/"),
	cpu_mask_remaining(def_cpu_mask), sensor_mask(0), power_clamp_index( - 1),
	intel_pstate_driver_index(-1),intel_rapl_index_limited(-1), msr_control_present(1)
	{
		for (int i=0; i<max_package_support; ++i)
			intel_rapl_index[i] = -1;
	}
	int read_thermal_zones();
	int read_cooling_devices();
	bool apply_cpu_operation(int cpu);
	void remove_cpu_mask_from_default_processing(unsigned int mask);
	unsigned int get_cpu_mask();
	int find_cdev_power_clamp();
	int find_cdev_rapl();
	int check_intel_p_state_driver();

	static const int max_package_support = 1024;
	static const int power_clamp_reduction_percent = 5;
	static const int msr_turbo_states_index = soft_cdev_start_index;
	static const int msr_p_states_index = soft_cdev_start_index + 1;
	static const int msr_p_states_index_limited = soft_cdev_start_index + 2;
	static const int turbo_on_off_index = soft_cdev_start_index + 3;
	static const int cpufreq_index = soft_cdev_start_index + 4;
	static const int t_state_index = soft_cdev_start_index + 5;
	static const int intel_pstate_control_index = soft_cdev_start_index + 6;
	static const int intel_rapl_limited_control_index = soft_cdev_start_index + 7;
	static const int msr_rapl_index = soft_cdev_start_index + 8;
	static const int package_index_multiplier = 2; // Each cdev for different cdev will be at
													// soft_cdev_start_index * package_index_multiplier

	int power_clamp_index; // dynamic based on thermal cdev
	int intel_pstate_driver_index;
	int intel_rapl_index[max_package_support];
	int intel_rapl_index_limited; // same function as intel_rapl_index, but limited states
	int msr_control_present;

};


#endif /* THD_ENGINE_DTS_H_ */
