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

int cthd_cdev::thd_cdev_exponential_controller(int set_point, int target_temp,
		int temperature, int state, int zone_id) {

	curr_state = get_curr_state();
	if ((min_state < max_state && curr_state < min_state)
			|| (min_state > max_state && curr_state > min_state))
		curr_state = min_state;
	max_state = get_max_state();
	thd_log_debug("thd_cdev_set_%d:curr state %d max state %d\n", index,
			curr_state, max_state);
	if (state) {
		if ((min_state < max_state && curr_state < max_state)
				|| (min_state > max_state && curr_state > max_state)) {
			int state = curr_state + inc_dec_val;
			if (trend_increase) {
				if (curr_pow == 0)
					base_pow_state = curr_state;
				++curr_pow;
				state = base_pow_state + int_2_pow(curr_pow) * inc_dec_val;
				thd_log_info(
						"cdev index:%d consecutive call, increment exponentially state %d\n",
						index, state);
				if ((min_state < max_state && state >= max_state)
						|| (min_state > max_state && state <= max_state)) {
					state = max_state;
					curr_pow = 0;
					curr_state = max_state;
				}
			} else {
				curr_pow = 0;
			}
			trend_increase = true;
			if ((min_state < max_state && state > max_state)
					|| (min_state > max_state && state < max_state))
				state = max_state;
			thd_log_debug("op->device:%s %d\n", type_str.c_str(), state);
			set_curr_state(state, zone_id);
		}
	} else {
		curr_pow = 0;
		trend_increase = false;
		if (((min_state < max_state && curr_state > min_state)
				|| (min_state > max_state && curr_state < min_state))
				&& auto_down_adjust == false) {
			int state = curr_state - inc_dec_val;
			if ((min_state < max_state && state < min_state)
					|| (min_state > max_state && state > min_state))
				state = min_state;
			thd_log_debug("op->device:%s %d\n", type_str.c_str(), state);
			set_curr_state(state, zone_id);
		} else {
			thd_log_debug("op->device: force min %s %d\n", type_str.c_str(),
					min_state);
			set_curr_state(min_state, zone_id);
		}
	}

	thd_log_info(
			"Set : threshold:%d, temperature:%d, cdev:%d(%s), curr_state:%d, max_state:%d\n",
			set_point, temperature, index, type_str.c_str(), get_curr_state(),
			max_state);

	thd_log_debug("<<thd_cdev_set_state %d\n", state);

	return THD_SUCCESS;
}

static bool sort_clamp_values(zone_trip_limits_t limit_1,
		zone_trip_limits_t limit_2) {
	return (limit_1.target_value < limit_2.target_value);
}

/*
 * How the state is set?
 * If the state set is called before debounce interval, then simply return
 * success We have two mask zone_mask and trip_mask. The idea is that while
 * any zone and trip has turned on then the other zone or trip can't reset
 * unless it is the last one.
 * In addition a zone trip can call for a particular target value for this
 * cdev (When it doesn't want the exponential or pid control to use, this
 * is true when multiple trips wants to use the cdev with different state
 * values. They way we support this:
 * When a valid target value is set then we push to a list, sorted using
 * increasing target values. The passed target value is set, no check is
 * done here to check the state higher/lower than the current state. This
 * is done during trip_point_check class.
 * When off is called for device, then we check if the zone in our list,
 * if yes, we remove this zone and set the next higher value state from
 * the list. If this is the current zone is the last then we remove
 * the zone and set the state to minimum state.
 */

int cthd_cdev::thd_cdev_set_state(int set_point, int target_temp,
		int temperature, int state, int zone_id, int trip_id,
		int target_value) {

	time_t tm;
	int ret;

	time(&tm);
	thd_log_debug(">>thd_cdev_set_state index:%d state:%d :%d:%d:%d\n", index,
			state, zone_id, trip_id, target_value);
	if (last_state == state && (tm - last_action_time) <= debounce_interval) {
		thd_log_debug(
				"Ignore: delay < debounce interval : %d, %d, %d, %d, %d\n",
				set_point, temperature, index, get_curr_state(), max_state);
		return THD_SUCCESS;
	}
	last_state = state;
	if (state) {
		zone_mask |= (1 << zone_id);
		trip_mask |= (1 << trip_id);

		if (target_value != TRIP_PT_INVALID_TARGET_STATE) {
			zone_trip_limits_t limit;
			bool found = false;

			for (unsigned int i = 0; i < zone_trip_limits.size(); ++i) {
				if (zone_trip_limits[i].zone == zone_id
						&& zone_trip_limits[i].trip == trip_id) {
					found = true;
					break;
				}
			}
			if (!found) {
				limit.zone = zone_id;
				limit.trip = trip_id;
				limit.target_value = target_value;
				thd_log_debug("Added zone %d trip %d clamp %d\n", limit.zone,
						limit.trip, limit.target_value);
				zone_trip_limits.push_back(limit);
				std::sort(zone_trip_limits.begin(), zone_trip_limits.end(),
						sort_clamp_values);
			}
			set_curr_state_raw(target_value, zone_id);
			curr_state = target_value;
			last_action_time = tm;
			thd_log_info(
					"Set : threshold:%d, temperature:%d, cdev:%d(%s), curr_state:%d, max_state:%d\n",
					set_point, temperature, index, type_str.c_str(),
					get_curr_state(), max_state);
			return THD_SUCCESS;
		}
	} else {

		if (zone_mask & (1 << zone_id)) {
			if (trip_mask & (1 << trip_id)) {
				trip_mask &= ~(1 << trip_id);
				zone_mask &= ~(1 << zone_id);
			}
		}

		if (zone_trip_limits.size() > 0) {
			int length = zone_trip_limits.size();
			int i;

			// Just remove the current zone requesting to turn off
			for (i = 0; i < length; ++i) {
				if (zone_trip_limits[i].zone == zone_id
						&& zone_trip_limits[i].trip == trip_id) {
					zone_trip_limits.erase(zone_trip_limits.begin() + i);
					thd_log_debug("Erased  [%d: %d\n", zone_id, trip_id);
					break;
				}
			}
			zone_trip_limits_t limit;

			if (zone_trip_limits.size() == 0) {
				limit.target_value = get_min_state();
				limit.zone = zone_id;
				limit.trip = trip_id;
			} else {
				limit = zone_trip_limits[zone_trip_limits.size() - 1];
			}

			if (cmp_current_state(limit.target_value) < 0) {
				thd_log_info(
						"new active zone; next in line  %d trip %d clamp %d\n",
						limit.zone, limit.trip, limit.target_value);
				set_curr_state_raw(limit.target_value, zone_id);
				thd_log_info(
						"Set : threshold:%d, temperature:%d, cdev:%d(%s), curr_state:%d, max_state:%d\n",
						set_point, temperature, index, type_str.c_str(),
						get_curr_state(), max_state);
			}
			return THD_SUCCESS;

		} else if (zone_mask != 0 || trip_mask != 0) {
			thd_log_debug(
					"skip to reduce current state as this is controlled by two zone or trip actions and one is still on %lx:%lx\n",
					zone_mask, trip_mask);
			return THD_SUCCESS;
		}
	}
	last_action_time = tm;

	curr_state = get_curr_state();
	if (curr_state == get_min_state()) {
		control_begin();
	}
	if (pid_enable) {
		pid_ctrl.set_target_temp(target_temp);
		ret = pid_ctrl.pid_output(temperature);
		ret += get_curr_state();
		if (ret > get_max_state())
			ret = get_max_state();
		if (ret < get_min_state())
			ret = get_min_state();
		set_curr_state_raw(ret, zone_id);
		thd_log_debug("Set : %d, %d, %d, %d, %d\n", set_point, temperature,
				index, get_curr_state(), max_state);
		ret = THD_SUCCESS;
	} else {
		ret = thd_cdev_exponential_controller(set_point, target_temp,
				temperature, state, zone_id);
	}
	if (curr_state == get_max_state()) {
		control_end();
	}

	return ret;
}
;

int cthd_cdev::thd_cdev_set_min_state(int zone_id) {
	zone_mask &= ~(1 << zone_id);
	if (zone_mask != 0) {
		thd_log_debug(
				"skip to reduce current state as this is controlled by two zone actions and one is still on %x\n",
				(unsigned int) zone_mask);
		return THD_SUCCESS;
	}
	trend_increase = false;
	set_curr_state(min_state, zone_id);

	return THD_SUCCESS;
}
