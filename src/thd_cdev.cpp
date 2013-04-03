/*
 * thd_cdev.cpp: thermal cooling class implementation
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

/*
 * This is parent class for all cooling devices. Most of the functions
 * are implemented in interface file except set state.
 * set_state uses the interface to get the current state, max state and
 * set the device state.
 * When state = 0, it causes reduction in the cooling device state
 * when state = 1, it increments cooling device state
 * When increments, it goes step by step, unless it finds that the temperature
 * can't be controlled by previous state with in def_poll_interval, otherwise
 * it will increase exponentially.
 * Reduction is always uses step by step to reduce ping pong affect.
 *
 */

#include "thd_cdev.h"
#include "thd_engine.h"

int cthd_cdev::thd_cdev_set_state(int set_point, int temperature, int state, int arg)
{
	thd_log_debug(">>thd_cdev_set_state index:%d state:%d\n", index, state);
	curr_state = get_curr_state();
	if(curr_state < min_state)
		curr_state = min_state;
	max_state = get_max_state();
	thd_log_debug("thd_cdev_set_%d:curr state %d max state %d\n", index,
curr_state, max_state);
	if(state)
	{
		if(curr_state < max_state)
		{
			int state = curr_state + inc_dec_val;
			time_t tm;
			time(&tm);
			if((tm - last_op_time) < (thd_engine->def_poll_interval/1000 + 1))
			{
				if(curr_pow == 0)
					base_pow_state = curr_state;
				++curr_pow;
				state = base_pow_state + int_2_pow(curr_pow) * inc_dec_val;
				thd_log_debug("consecutive call, increment exponentially state %d\n",
state);
				if(state >= max_state)
				{
					state = max_state;
					curr_pow = 0;
					curr_state = max_state;
					last_op_time = tm;
				}
			}
			else
				curr_pow = 0;
			last_op_time = tm;
			sensor_mask |= arg;
			if(state > max_state)
				state = max_state;
			thd_log_debug("op->device:%s %d\n", type_str.c_str(), state);
			set_curr_state(state, arg);
		}
	}
	else
	{
		curr_pow = 0;
		if(curr_state > 0 && auto_down_adjust == false)
		{
			int state = curr_state - inc_dec_val;
			sensor_mask &= ~arg;
			if(sensor_mask != 0)
			{
				thd_log_debug(
"skip to reduce current state as this is controlled by two sensor actions and one is still on %x\n", sensor_mask);
			}
			else
			{
				if(state < min_state)
					state = min_state;
				thd_log_debug("op->device:%s %d\n", type_str.c_str(), state);
				set_curr_state(state, arg);
			}
		}
		else
		{
			set_curr_state(min_state, arg);
		}

	}
	thd_log_info("Set : %d, %d, %d, %d, %d\n", set_point, temperature, index,
get_curr_state(), max_state);

	thd_log_debug("<<thd_cdev_set_state %d\n", state);

	return THD_SUCCESS;
};
