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
#include <vector>
#include "thd_cdev_tstates.h"
#include "thd_engine_dts.h"

void cthd_cdev_tstates::set_curr_state(int state, int arg)
{
	if (cpu_index == -1) {
		int cpus = msr.get_no_cpus();
		for (int i=0; i<cpus; ++i) {
			if (thd_engine->apply_cpu_operation(i) == false)
				continue;
			msr.set_clock_mod_duty_cycle_per_cpu(i, state);
			curr_state = state;
		}
	}
	else {
		if (thd_engine->apply_cpu_operation(cpu_index) == true) {
			msr.set_clock_mod_duty_cycle_per_cpu(cpu_index, state);
			curr_state = state;
		}
	}
}

int cthd_cdev_tstates::get_max_state()
{
	return t_states_cnt;
}


