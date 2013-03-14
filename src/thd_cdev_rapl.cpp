/*
 * cthd_cdev_rapl.cpp: thermal cooling class implementation
 *	using RAPL
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
#include "thd_cdev_rapl.h"

void cthd_sysfs_cdev_rapl::set_curr_state(int state, int arg)
{

	std::stringstream tc_state_dev;
	tc_state_dev << "cooling_device" << index << "/cur_state";
	if(cdev_sysfs.exists(tc_state_dev.str()))
	{
		std::stringstream state_str;
		int new_state;

		if (state < inc_dec_val)
		{
			new_state = 0;
			curr_state = 0;
		}
		else
		{
			new_state = max_state - state;
			curr_state = state;
		}
		state_str << new_state;
		thd_log_debug("set cdev state index %d state %d\n", index, state);
		if (cdev_sysfs.write(tc_state_dev.str(), state_str.str()) < 0)
			curr_state = (state == 0) ? 0 : max_state;
	}
	else
		curr_state = 0;

}

int cthd_sysfs_cdev_rapl::get_curr_state()
{
	return curr_state;
}
