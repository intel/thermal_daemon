/* thd_msr.h: thermal engine msr class interface
 *
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

#ifndef THD_MSR_H
#define THD_MSR_H

#include "thd_common.h"
#include "thd_sys_fs.h"

class cthd_msr {
private:
	csys_fs msr_sysfs;
	int no_of_cpus;

public:
	cthd_msr();

	int read_msr(int cpu, unsigned int idx, unsigned long long *val);
	int write_msr(int cpu, unsigned int idx, unsigned long long val);

	int get_no_cpus();

	bool check_turbo_status();
	int enable_turbo();
	int disable_turbo();

	int get_clock_mod_duty_cycle();
	int set_clock_mod_duty_cycle(int state);

	int get_min_freq();
	int get_max_freq();
	int get_min_turbo_freq();
	int get_max_turbo_freq();

	int inc_freq_state();
	int dec_freq_state();
	int set_freq_state(int state);

	int set_perf_bias_performace();
	int set_perf_bias_balaced();
	int set_perf_bias_energy();

	int get_mperf_value(int cpu, unsigned long long *value);
	int get_aperf_value(int cpu, unsigned long long *value);

	int set_freq_state_per_cpu(int cpu, int state);
	int inc_freq_state_per_cpu(int cpu);
	int dec_freq_state_per_cpu(int cpu);
	int set_clock_mod_duty_cycle_per_cpu(int cpu, int state);
	int disable_turbo_per_cpu(int cpu);
	int enable_turbo_per_cpu(int cpu);

};

#endif
