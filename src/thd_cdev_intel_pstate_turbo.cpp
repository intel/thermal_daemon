/*
 * thd_cdev_intel_pstate_turbo.cpp: thermal cooling class implementation
 *	using Turbo states for Intel P State driver
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

/* This implementation allow to engage disengage turbo state.
 *
 */

#include <string>
#include <vector>
#include "thd_cdev_intel_pstate_turbo.h"

void cthd_cdev_intel_pstate_turbo::set_curr_state(int state, int arg)
{
	std::stringstream tc_state_dev;
	int new_state;

	tc_state_dev << "/no_turbo";
	if(cdev_sysfs.exists(tc_state_dev.str()))
	{
		std::stringstream state_str;
		if (state)
			new_state = 1;
		else
		{
			new_state = 0;
		}
		state_str << new_state;
		thd_log_debug("set cdev state index %d state %d percent %d\n", index, state, new_state);
		if (cdev_sysfs.write(tc_state_dev.str(), state_str.str()) < 0)
			curr_state = (state == 0) ? 0 : max_state;
		else
			curr_state = state;
	}
	else
		curr_state = (state == 0) ? 0 : max_state;
}

int cthd_cdev_intel_pstate_turbo::get_max_state()
{
	return turbo_states_cnt;
}

int cthd_cdev_intel_pstate_turbo::update()
{
	std::stringstream tc_state_dev;

	tc_state_dev << "/no_turbo";
	if(!cdev_sysfs.exists(tc_state_dev.str()))
	{
		thd_log_debug("intel p state turbo not found \n");
		return THD_ERROR;
	}
	max_state = turbo_states_cnt;
	return THD_SUCCESS;
}
