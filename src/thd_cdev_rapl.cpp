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
	max_state = max_state/rapl_low_limit_ratio;
	thd_log_debug("RAPL max limit %d increment: %d\n", max_state, inc_dec_val);

	// Set time window to match polling interval
	std::stringstream tm_window_dev;
	tm_window_dev << "cooling_device" << index << "/device/time_window1";
	if(cdev_sysfs.exists(tm_window_dev.str()))
	{
		std::stringstream tm_window;
		tm_window << (thd_engine->get_poll_timeout_ms() - 200); // 200 ms before poll timeout
		thd_log_debug("RAPL time_window1: %d\n", thd_engine->get_poll_timeout_ms() - 200);
		if (cdev_sysfs.write(tm_window_dev.str(), tm_window.str()) < 0)
		{
			thd_log_warn("Can't set RAPL time_window1\n");
		}
	}

	return ret;
}
