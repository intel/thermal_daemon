/*
 * thd_cdev_pstates.cpp: thermal cooling class implementation
 *	using Turbo states
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
#include <vector>
#include "thd_cdev_turbo_states.h"

void cthd_cdev_turbo_states::set_curr_state(int state, int arg)
{
	int ret = THD_ERROR;

	if(state == 1)
	{
		if(cpu_index ==  - 1)
			ret = msr.disable_turbo();
		else
			ret = msr.disable_turbo_per_cpu(cpu_index);
	}
	else
	{
		if(cpu_index ==  - 1)
			ret = msr.enable_turbo();
		else
			ret = msr.enable_turbo_per_cpu(cpu_index);
	}
	if (ret == THD_SUCCESS)
		curr_state = state;
	else
		curr_state = (state == 0) ? 0 : max_state;
}

int cthd_cdev_turbo_states::get_max_state()
{
	return turbo_states_cnt;
}
