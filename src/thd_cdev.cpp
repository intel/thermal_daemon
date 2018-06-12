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
						"cdev index:%d consecutive call, increment exponentially state %d (min %d max %d)\n",
						index, state, min_state, max_state);
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

static bool sort_clamp_values_asc(zone_trip_limits_t limit_1,
		zone_trip_limits_t limit_2) {
	return (limit_1.target_value < limit_2.target_value);
}

static bool sort_clamp_values_dec(zone_trip_limits_t limit_1,
		zone_trip_limits_t limit_2) {
	return (limit_1.target_value > limit_2.target_value);
}

/*
 * How the state is set?
 * If the state set is called before debounce interval, then simply return
 * success.
 * It is possible that same cdev is used by two trips in the same zone or
 * different zone. So if one trip activated a cdev (state = 1), even if the
 * other trip calls for deactivation (state = 0), the trip shouldn't be
 * deactivated. This is implemented by adding each activation to push the
 * zone, trip and target_value to a list if not present and remove on
 * call for deactivation. After deleting from the list if the list still
 * has members, that means that this device is still under activation from
 * some other trip. So in this case simply return without changing the state
 * of cdev.
 *
 * Special handling when a target value is passed:
 *
 * In addition a zone trip can call for a particular target value for this
 * cdev (When it doesn't want the exponential or pid control to use, this
 * is true when multiple trips wants to use the cdev with different state
 * values. They way we support this:
 * When a valid target value is set then we push to the list, sorted using
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
		int target_state_valid, int target_value,
		pid_param_t *pid_param, cthd_pid& pid, bool force) {

	time_t tm;
	int ret;

	time(&tm);
	thd_log_info(
			">>thd_cdev_set_state index:%d state:%d :%d:%d:%d:%d force:%d\n",
			index, state, zone_id, trip_id, target_state_valid, target_value,
			force);
	if (!force && last_state == state && state
			&& (tm - last_action_time) <= debounce_interval) {
		thd_log_debug(
				"Ignore: delay < debounce interval : %d, %d, %d, %d, %d\n",
				set_point, temperature, index, get_curr_state(), max_state);
		return THD_SUCCESS;
	}

	last_state = state;

	if (state) {
		bool found = false;

		// Search for the zone and trip id in the list
		for (unsigned int i = 0; i < zone_trip_limits.size(); ++i) {
			if (zone_trip_limits[i].zone == zone_id
					&& zone_trip_limits[i].trip == trip_id) {
				found = true;
				break;
			}
		}
		// If not found in the list add to the list
		if (!found) {
			zone_trip_limits_t limit;

			limit.zone = zone_id;
			limit.trip = trip_id;
			limit.target_state_valid = target_state_valid;
			limit.target_value = target_value;
			thd_log_info("Added zone %d trip %d clamp_valid %d clamp %d\n",
					limit.zone, limit.trip, limit.target_state_valid,
					limit.target_value);
			zone_trip_limits.push_back(limit);

			if (target_state_valid) {
				if (min_state < max_state) {
					std::sort(zone_trip_limits.begin(), zone_trip_limits.end(),
							sort_clamp_values_asc);
				} else {
					std::sort(zone_trip_limits.begin(), zone_trip_limits.end(),
							sort_clamp_values_dec);
				}
			}
		}
	} else {
		thd_log_debug("zone_trip_limits.size() %zu\n", (size_t)zone_trip_limits.size());
		if (zone_trip_limits.size() > 0) {
			int length = zone_trip_limits.size();
			int _target_state_valid = 0;
			int i;
			int erased = 0;

			// Just remove the current zone requesting to turn off
			for (i = 0; i < length; ++i) {
				thd_log_info("match zone %d trip %d clamp_valid %d clamp %d\n",
						zone_trip_limits[i].zone, zone_trip_limits[i].trip,
						zone_trip_limits[i].target_state_valid,
						zone_trip_limits[i].target_value);

				if (zone_trip_limits[i].zone == zone_id
						&& zone_trip_limits[i].trip == trip_id
						&& target_state_valid
								== zone_trip_limits[i].target_state_valid
						&& target_value == zone_trip_limits[i].target_value) {
					_target_state_valid = zone_trip_limits[i].target_state_valid;
					zone_trip_limits.erase(zone_trip_limits.begin() + i);
					thd_log_info("Erased  [%d: %d %d\n", zone_id, trip_id,
							target_value);
					erased = 1;
					break;
				}
			}

			if (zone_trip_limits.size()) {
				zone_trip_limits_t limit;

				limit = zone_trip_limits[zone_trip_limits.size() - 1];
				target_value = limit.target_value;
				target_state_valid = limit.target_state_valid;
				zone_id = limit.zone;
				trip_id = limit.trip;
				if (!erased)
				{
					thd_log_info(
							"Currently active limit by [%d: %d %d %d]: ignore\n",
							zone_id, trip_id, target_state_valid, target_value);
					return THD_SUCCESS;
				}

			} else if (force) {
				thd_log_info("forced to min_state \n");
			} else {
				// If the deleted entry has a target then on deactivation
				// set the state to min_state
				if (_target_state_valid)
					target_value = get_min_state();
			}
		} else {
			target_state_valid = 0;
		}
	}

	last_action_time = tm;

	curr_state = get_curr_state();
	if (curr_state == get_min_state()) {
		control_begin();
	}

	if (target_state_valid) {
		set_curr_state_raw(target_value, zone_id);
		curr_state = target_value;
		ret = THD_SUCCESS;
		thd_log_info("Set : %d, %d, %d, %d, %d\n", set_point, temperature,
				index, get_curr_state(), max_state);

	} else if (pid_param && pid_param->valid) {
		// Handle PID param unique to a trip
		pid.set_target_temp(target_temp);
		ret = pid.pid_output(temperature);
		ret += get_min_state();
		if (get_min_state() < get_max_state()) {
			if (ret > get_max_state())
				ret = get_max_state();
			if (ret < get_min_state())
				ret = get_min_state();
		} else {
			if (ret < get_max_state())
				ret = get_max_state();
			if (ret > get_min_state())
				ret = get_min_state();
		}
		set_curr_state_raw(ret, zone_id);
		thd_log_info("Set pid : %d, %d, %d, %d, %d\n", set_point, temperature,
				index, get_curr_state(), max_state);
		ret = THD_SUCCESS;

		if (state == 0)
			pid.reset();

	} else if (pid_enable) {
		// Handle PID param common to whole cooling device
		pid_ctrl.set_target_temp(target_temp);
		ret = pid_ctrl.pid_output(temperature);
		ret += get_min_state();

		if (get_min_state() < get_max_state()) {
			if (ret > get_max_state())
				ret = get_max_state();
			if (ret < get_min_state())
				ret = get_min_state();
		} else {
			if (ret < get_max_state())
				ret = get_max_state();
			if (ret > get_min_state())
				ret = get_min_state();
		}

		set_curr_state_raw(ret, zone_id);
		thd_log_info("Set : %d, %d, %d, %d, %d\n", set_point, temperature,
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

int cthd_cdev::thd_cdev_set_min_state(int zone_id, int trip_id) {
	trend_increase = false;
	cthd_pid unused;
	thd_cdev_set_state(0, 0, 0, 0, zone_id, trip_id, 1, min_state, NULL, unused, true);

	return THD_SUCCESS;
}
