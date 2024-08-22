/*
 * thd_trip_point.cpp: thermal zone class implementation
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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/reboot.h>
#include "thd_trip_point.h"
#include "thd_engine.h"

cthd_trip_point::cthd_trip_point(int _index, trip_point_type_t _type, unsigned
int _temp, unsigned int _hyst, int _zone_id, int _sensor_id,
		trip_control_type_t _control_type) :
		index(_index), type(_type), temp(_temp), hyst(_hyst), control_type(
				_control_type), zone_id(_zone_id), sensor_id(_sensor_id), trip_on(
				false), poll_on(false), depend_cdev(NULL), depend_cdev_state(0), depend_cdev_state_rel(
				EQUAL), crit_trip_count(0) {
	thd_log_debug("Add trip pt %d:%d:0x%x:%d:%d\n", type, zone_id, sensor_id,
			temp, hyst);
}

void cthd_trip_point::set_dependency(std::string cdev, std::string state_str)
{
	cthd_cdev *cdev_ptr;

	cdev_ptr = thd_engine->search_cdev(std::move(cdev));
	if (cdev_ptr) {
		int match;
		int state_index = 0;

		depend_cdev = cdev_ptr;

		match = state_str.compare(0, 2, "==");
		if (!match) {
			depend_cdev_state_rel = EQUAL;
			state_index = 2;
		}

		match = state_str.compare(0, 1, ">");
		if (!match) {
			depend_cdev_state_rel = GREATER;
			state_index = 1;
		}

		match = state_str.compare(0, 1, "<");
		if (!match) {
			state_index = 1;
			depend_cdev_state_rel = LESSER;
		}

		match = state_str.compare(0, 2, "<=");
		if (!match) {
			depend_cdev_state_rel = LESSER_OR_EQUAL;
			state_index = 2;
		}

		match = state_str.compare(0, 2, ">=");
		if (!match) {
			depend_cdev_state_rel = GREATER_OR_EQUAL;
			state_index = 2;
		}

		depend_cdev_state = atoi(state_str.substr(state_index).c_str());

	}
}

bool cthd_trip_point::thd_trip_point_check(int id, unsigned int read_temp,
		int pref, bool *reset) {
	int on = -1;
	int off = -1;
	bool apply = false;

	*reset = false;

	if (type == INVALID_TRIP_TYPE)
		return false;

	if (depend_cdev && read_temp >= temp) {
		int _state = depend_cdev->get_curr_state();
		int valid = 0;

		switch (depend_cdev_state_rel) {
		case EQUAL:
			if (_state == depend_cdev_state)
				valid = 1;
			break;
		case GREATER:
			if (_state > depend_cdev_state)
				valid = 1;
			break;
		case LESSER:
			if (_state < depend_cdev_state)
				valid = 1;
			break;
		case LESSER_OR_EQUAL:
			if (_state <= depend_cdev_state)
				valid = 1;
			break;
		case GREATER_OR_EQUAL:
			if (_state >= depend_cdev_state)
				valid = 1;
			break;
		default:
			break;
		}

		if (!valid) {
			thd_log_info("constraint failed %s:%d:%d:%d\n",
					depend_cdev->get_cdev_type().c_str(), _state, depend_cdev_state_rel,
					depend_cdev_state);
			return false;
		}
	}

	if (sensor_id != DEFAULT_SENSOR_ID && sensor_id != id)
		return false;

	if (read_temp == 0) {
		thd_log_debug("TEMP == 0 pref: %d\n", pref);
	}

	if (type == CRITICAL) {

		if (!ignore_critical && read_temp >= temp) {
			thd_log_warn("critical temp reached\n");
			if (crit_trip_count < consecutive_critical_events) {
				++crit_trip_count;
				return true;
			}
			crit_trip_count = 0;
			sync();
#ifdef ANDROID
			int ret;

			ret = property_set("sys.powerctl", "shutdown,thermal");
			if (ret != 0)
				thd_log_warn("power off failed ret=%d err=%s\n",
					     ret, strerror(errno));
			else
				thd_log_warn("power off initiated\n");
#else
			thd_log_warn("power off initiated\n");
			reboot(RB_POWER_OFF);
#endif

			return true;
		}
		crit_trip_count = 0;
	}
	if (type == HOT) {
		if (!ignore_critical && read_temp >= temp) {
			thd_log_warn("Hot temp reached\n");
			if (crit_trip_count < consecutive_critical_events) {
				++crit_trip_count;
				return true;
			}
			crit_trip_count = 0;
			thd_log_warn("Hot temp reached\n");
			csys_fs power("/sys/power/");
			power.write("state", "mem");
			return true;
		}
		crit_trip_count = 0;
	}

	if (type == POLLING && sensor_id != DEFAULT_SENSOR_ID) {
		cthd_sensor *sensor = thd_engine->get_sensor(sensor_id);
		if (sensor) {
			if (!poll_on && read_temp >= temp) {
				thd_log_debug("polling trip reached, on\n");
				sensor->sensor_poll_trip(true);
				poll_on = true;
				sensor->sensor_fast_poll(true);
				if (sensor->check_async_capable())
					sensor->set_threshold(0, temp);
			} else if (poll_on && read_temp < temp) {
				sensor->sensor_poll_trip(false);
				thd_log_debug("Dropped below poll threshold\n");
				*reset = true;
				poll_on = false;
				sensor->sensor_fast_poll(false);
				if (sensor->check_async_capable())
					sensor->set_threshold(0, temp);
			}
		}
		return true;
	}
	thd_log_debug("pref %d type %d temp %d trip %d\n", pref, type, read_temp,
			temp);
	switch (pref) {
	case PREF_DISABLED:
		return false;
		break;

	case PREF_PERFORMANCE:
		if (type == ACTIVE || type == MAX) {
			apply = true;
			thd_log_debug("Active Trip point applicable\n");
		}
		break;

	case PREF_ENERGY_CONSERVE:
		if (type == PASSIVE || type == MAX) {
			apply = true;
			thd_log_debug("Passive Trip point applicable\n");
		}
		break;

	default:
		break;
	}

	if (apply) {
		if (read_temp >= temp) {
			thd_log_debug("Trip point applicable >  %d:%d\n", index, temp);
			on = 1;
			trip_on = true;
		} else if ((trip_on && (read_temp + hyst) < temp)
				|| (!trip_on && read_temp < temp)) {
			thd_log_debug("Trip point applicable <  %d:%d\n", index, temp);
			off = 1;
			trip_on = false;
		}
	} else
		return false;

	if (on != 1 && off != 1)
		return true;

	int i, ret;
	thd_log_debug("cdev size for this trippoint %lu\n",
			(unsigned long) cdevs.size());
	if (on > 0) {
		for (unsigned i = 0; i < cdevs.size(); ++i) {
			cthd_cdev *cdev = cdevs[i].cdev;

			if (cdevs[i].sampling_priod) {
				time_t tm;
				time(&tm);
				if ((tm - cdevs[i].last_op_time) < cdevs[i].sampling_priod) {
					thd_log_debug("Too early to act zone:%d index %d tm %jd\n",
							zone_id, cdev->thd_cdev_get_index(),
							(intmax_t)tm - cdevs[i].last_op_time);
					break;
				}
				cdevs[i].last_op_time = tm;
			}
			thd_log_debug("cdev at index %d:%s\n", cdev->thd_cdev_get_index(),
					cdev->get_cdev_type().c_str());
			/*
			 * When the cdev is already in max state, we skip this cdev.
			 */
			if (cdev->in_max_state()) {
				thd_log_debug("Need to switch to next cdev target %d\n",
						cdev->map_target_state(cdevs[i].target_state_valid,
								cdevs[i].target_state));
				// No scope of control with this cdev
				continue;
			}

			if (cdevs[i].target_state == TRIP_PT_INVALID_TARGET_STATE)
				cdevs[i].target_state = cdev->get_min_state();

			ret = cdev->thd_cdev_set_state(temp, temp, read_temp, (type == MAX),
					1, zone_id, index, cdevs[i].target_state_valid,
					cdev->map_target_state(cdevs[i].target_state_valid,
							cdevs[i].target_state), &cdevs[i].pid_param,
					cdevs[i].pid, false, cdevs[i].min_max_valid,
					cdevs[i].min_state, cdevs[i].max_state);
			if (control_type == SEQUENTIAL && ret == THD_SUCCESS) {
				// Only one cdev activation
				break;
			}
		}
	}

	if (off > 0) {
		for (i = cdevs.size() - 1; i >= 0; --i) {

			cthd_cdev *cdev = cdevs[i].cdev;
			thd_log_debug("cdev at index %d:%s\n", cdev->thd_cdev_get_index(),
					cdev->get_cdev_type().c_str());


			if (cdev->in_min_state()) {
				thd_log_debug("Need to switch to next cdev\n");
				// No scope of control with this cdev
				continue;
			}

			if (cdevs[i].target_state == TRIP_PT_INVALID_TARGET_STATE)
				cdevs[i].target_state = cdev->get_min_state();

			cdev->thd_cdev_set_state(temp, temp, read_temp, (type == MAX), 0,
					zone_id, index, cdevs[i].target_state_valid,
					cdev->map_target_state(cdevs[i].target_state_valid,
							cdevs[i].target_state), &cdevs[i].pid_param,
					cdevs[i].pid, false, cdevs[i].min_max_valid,
					cdevs[i].min_state, cdevs[i].max_state);

				if (control_type == SEQUENTIAL) {
					// Only one cdev activation
					break;
				}
		}
	}

	return true;
}

void cthd_trip_point::thd_trip_point_add_cdev(cthd_cdev &cdev, int influence,
		int sampling_period, int target_state_valid, int target_state,
		pid_param_t *pid_param, int min_max_valid, int min_state,
		int max_state) {
	trip_pt_cdev_t thd_cdev = {};
	thd_cdev.cdev = &cdev;
	thd_cdev.influence = influence;
	thd_cdev.sampling_priod = sampling_period;
	thd_cdev.last_op_time = 0;
	thd_cdev.target_state_valid = target_state_valid;
	thd_cdev.target_state = target_state;

	thd_cdev.min_max_valid = min_max_valid;

	thd_log_info("min:%d max:%d\n", min_state, max_state);
	if (min_max_valid) {
		thd_cdev.min_state = min_state;
		thd_cdev.max_state = max_state;
	}
	if (pid_param && pid_param->valid) {
		thd_log_info("pid valid %f:%f:%f\n", pid_param->kp, pid_param->ki,
				pid_param->kd);
		memcpy(&thd_cdev.pid_param, pid_param, sizeof(pid_param_t));
		thd_cdev.pid.set_pid_param(pid_param->kp, pid_param->ki, pid_param->kd);
	} else {
		memset(&thd_cdev.pid_param, 0, sizeof(pid_param_t));
	}
	trip_cdev_add(thd_cdev);
}

int cthd_trip_point::thd_trip_point_add_cdev_index(int _index, int influence) {
	cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(_index);
	if (cdev) {
		trip_pt_cdev_t thd_cdev = {};
		thd_cdev.cdev = cdev;
		thd_cdev.influence = influence;
		thd_cdev.sampling_priod = 0;
		thd_cdev.last_op_time = 0;
		thd_cdev.target_state_valid = 0;
		trip_cdev_add(thd_cdev);
		return THD_SUCCESS;
	} else {
		thd_log_warn("thd_trip_point_add_cdev_index not present %d\n", _index);
		return THD_ERROR;
	}
}

void cthd_trip_point::thd_trip_cdev_state_reset(int force) {
	thd_log_debug("thd_trip_cdev_state_reset\n");
	for (int i = cdevs.size() - 1; i >= 0; --i) {
		cthd_cdev *cdev = cdevs[i].cdev;
		thd_log_debug("thd_trip_cdev_state_reset index %d:%s\n",
				cdev->thd_cdev_get_index(), cdev->get_cdev_type().c_str());

		if (!force && cdev->in_min_state()) {
			thd_log_debug("Need to switch to next cdev\n");
			// No scope of control with this cdev
			continue;
		}
		cdev->thd_cdev_set_min_state(zone_id, index);
	}
}
