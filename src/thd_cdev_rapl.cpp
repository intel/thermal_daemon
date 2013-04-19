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
#include "thd_engine.h"

/* This uses Intel RAPL driver to cool the system. RAPL driver show
 * mas thermal spec power in max_state. Each state can compensate
 * rapl_power_dec_percent, from the max state.
 *
 */
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
			new_state = phy_max - state;
			curr_state = state;
		}
		state_str << new_state;
		thd_log_debug("set cdev state index %d state %d wr:%d\n", index, state, new_state);
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

int cthd_sysfs_cdev_rapl::get_max_state()
{
	return max_state;
}

int cthd_sysfs_cdev_rapl::update()
{
	int ret = cthd_sysfs_cdev::update();
	phy_max = max_state;
	set_inc_dec_value(phy_max * (float)rapl_power_dec_percent/100);

	max_state -= (float)max_state * rapl_low_limit_percent/100;
	thd_log_debug("RAPL max limit %d increment: %d\n", max_state, inc_dec_val);

	return ret;
}
