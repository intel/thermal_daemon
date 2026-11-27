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
				false), poll_on(false), depend_cdev(nullptr), depend_cdev_state(0), depend_cdev_state_rel(
				EQUAL), crit_trip_count(0) {
	thd_log_debug("Add trip pt %d:%d:0x%x:%d:%d\n", type, zone_id, sensor_id,
			temp, hyst);
}

void cthd_trip_point::set_dependency(const std::string& cdev, const std::string& state_str)
{
	cthd_cdev *cdev_ptr;

	cdev_ptr = thd_engine->search_cdev(cdev);
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

	if (is_step_control()) {
		step_control(cdevs[0], read_temp);
		return true;
	}

	if (on != 1 && off != 1) {
		bool pid_release = false;

		for (unsigned int j = 0; j < cdevs.size(); ++j) {
			if (cdevs[j].pid_param.valid && cdevs[j].pid_param.gradual_release)
				pid_release = true;
		}

		/*
		 * Within the hysteresis below an active trip: a PID with gradual
		 * release keeps controlling, otherwise its first call after
		 * leaving the band would act on the whole time spent in it.
		 */
		if (!trip_on || !pid_release)
			return true;
		on = 1;
	}

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
			 *
			 * EXCEPTION: For PID control on an inverted-range device
			 * (max_state < min_state, e.g. RAPL/SPEL power limits where
			 * max_state = minimum power = most restrictive), we must keep
			 * calling the PID even when at max_state.  The PID may compute
			 * a less-restrictive output as temperature drops back toward the
			 * trip threshold, allowing the power limit to be relaxed
			 * proportionally rather than staying pinned at max_state until
			 * the trip fully deactivates.
			 */
			if (cdev->in_max_state() &&
			    !((cdevs[i].pid_param.valid || cdev->is_pid_enabled()) &&
			      cdev->get_max_state() < cdev->get_min_state())) {
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
		thd_log_info("pid valid %f:%f:%f mode:%s\n", pid_param->kp,
				pid_param->ki, pid_param->kd,
				pid_param->mode == PID_INCREMENTAL ? "incremental" : "absolute");
		thd_cdev.pid_param = *pid_param;
		thd_cdev.pid.set_pid_param(pid_param->kp, pid_param->ki, pid_param->kd);
		thd_cdev.pid.set_pid_mode(pid_param->mode);
		thd_cdev.pid.set_time_scaled(pid_param->time_scaled);
	} else {
		thd_cdev.pid_param = pid_param_t();
		thd_cdev.pid_param.valid = 0;
		thd_cdev.pid_param.mode = PID_ABSOLUTE;
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

		if (cdevs[i].step_points.size()) {
			cdevs[i].step_active = false;
			cdevs[i].step_band = -1;
			cdev->thd_cdev_set_step_request(zone_id, index, 0, false);
			continue;
		}
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

void cthd_trip_point::thd_trip_point_add_step_point(cthd_cdev &cdev,
		const step_point_t &point, int sampling_period) {
	if (sampling_period < 0)
		sampling_period = 0;
	if (sampling_period > INT32_MAX / 100)
		sampling_period = INT32_MAX / 100;

	if (!cdevs.size()) {
		thd_trip_point_add_cdev(cdev, default_influence);
		cdevs[0].step_period_ms = sampling_period * 100;
		/* For the package power when reaching a point */
		thd_engine->rapl_power_meter.rapl_start_measure_power();
	}

	trip_pt_cdev_t &c = cdevs[0];
	auto it = c.step_points.begin();

	while (it != c.step_points.end() && it->temp <= point.temp)
		++it;
	c.step_points.insert(it, point);

	/* Sampling period of the most often sampled point */
	if (sampling_period * 100 < c.step_period_ms)
		c.step_period_ms = sampling_period * 100;

	temp = c.step_points[0].temp;
}

void cthd_trip_point::step_points_finalize() {
	if (!is_step_control())
		return;

	std::vector<step_point_t> &points = cdevs[0].step_points;
	step_point_t last = points.back();

	/*
	 * A last entry with limit MAX: full limit 1 C above it. Its limit
	 * setting value is PL1 max, so reaching that point doesn't set the
	 * request at once (see step_control()): the limit coefficient sets how
	 * fast it goes down.
	 */
	if (last.limit == STEP_LIMIT_NONE && last.temp <= INT32_MAX - 1000) {
		last.temp += 1000;
		last.limit = STEP_LIMIT_FULL;
		last.max_entry = 1;
		points.push_back(last);
	}
}

static long long monotonic_ms() {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int step_limit_state(cthd_cdev *cdev, int limit) {
	int lo = cdev->get_min_state(), hi = cdev->get_max_state();

	if (limit == STEP_LIMIT_NONE)
		return lo;
	if (limit == STEP_LIMIT_FULL)
		return hi;

	limit = cdev->map_target_state(1, limit);
	if (lo > hi)
		std::swap(lo, hi);
	if (limit < lo)
		limit = lo;
	if (limit > hi)
		limit = hi;

	return limit;
}

/*
 * Step control (PSVT): the limit for the current temperature is interpolated
 * between the curve points around it; no limit below the first point. Each
 * sampling period the request moves towards that limit by step * coeff:
 * limit_coeff when limiting further, unlimit_coeff when releasing. The
 * device applies the most restrictive request of all its step trips.
 *
 * On reaching a curve point (temperature rising past it) the request is set
 * at once to that point's limit if the package power is above it, else to
 * the package power (within the device range); stepping continues from
 * there in the following periods. Not for the full limit point after a
 * last MAX entry: the entry's limit setting value is PL1 max, the request
 * only steps.
 */
void cthd_trip_point::step_control(trip_pt_cdev_t &c, unsigned int read_temp) {
	cthd_cdev *cdev = c.cdev;
	const std::vector<step_point_t> &points = c.step_points;

	/* Preference update: no temperature */
	if (!read_temp)
		return;

	if (c.step_period_ms) {
		long long now = monotonic_ms();

		/* Allow for the poll timing jitter, the period has 0.1 s precision */
		if (c.step_last_ms && now - c.step_last_ms < c.step_period_ms - 100)
			return;
		c.step_last_ms = now;
	}

	int no_limit = cdev->get_min_state();
	/* 1 when more cooling means a higher state, -1 for power limits */
	int dir = (cdev->get_max_state() >= no_limit) ? 1 : -1;
	int band = -1;

	for (unsigned int i = 0; i < points.size(); ++i) {
		if ((long long) read_temp >= points[i].temp)
			band = i;
	}

	const step_point_t &p = points[band < 0 ? 0 : band];
	long long limit = no_limit;

	if (band >= 0) {
		limit = step_limit_state(cdev, p.limit);
		if (band + 1 < (int) points.size()) {
			const step_point_t &next = points[band + 1];
			long long next_limit = step_limit_state(cdev, next.limit);
			long long span = (long long) next.temp - p.temp;

			if (span > 0)
				limit += (next_limit - limit)
						* ((long long) read_temp - p.temp) / span;
		}
	}

	long long request = c.step_active ? c.step_request : no_limit;
	bool reached = (band > c.step_band);
	bool reached_power = false;

	c.step_band = band;

	/* Power limits only (dir < 0): states are power in uW. Not for the
	 * point after a MAX entry, whose limit setting value is PL1 max. */
	if (reached && dir < 0 && !p.max_entry) {
		long long point_limit = step_limit_state(cdev, p.limit);

		if (point_limit != no_limit && request > point_limit) {
			/*
			 * Measured each poll since the trip was added. Only before
			 * the first two samples this measures, waiting a second.
			 */
			long long power =
					thd_engine->rapl_power_meter.rapl_action_get_last_power(
							PACKAGE);
			if (!power) {
				power = thd_engine->rapl_power_meter.rapl_action_get_power(
						PACKAGE);
				if (c.step_period_ms)
					c.step_last_ms = monotonic_ms();
			}

			thd_log_debug("step zone:%d trip:%d reached %d, power:%lld\n",
					zone_id, index, p.temp, power);
			if (power > point_limit) {
				limit = request = point_limit;
			} else if (power > 0 && power < request) {
				/* Not below the device's lowest power limit */
				request = power;
				if (request < cdev->get_max_state())
					request = cdev->get_max_state();
				reached_power = true;
			}
		}
	}

	/* > 0: the request must limit further */
	long long more = (limit - request) * dir;
	long long delta;

	/* Started from the package power: step from there in the next period */
	if (reached_power)
		more = 0;

	if (more > 0) {
		delta = (long long) p.step * p.limit_coeff;
		if (delta > more)
			delta = more;
		request += delta * dir;
	} else if (more < 0) {
		delta = (long long) p.step * p.unlimit_coeff;
		if (delta > -more)
			delta = -more;
		request -= delta * dir;
	}

	bool active = (request != no_limit);

	if (!active && !c.step_active)
		return;

	thd_log_debug("step zone:%d trip:%d temp:%u limit:%lld request:%lld\n",
			zone_id, index, read_temp, limit, request);

	c.step_request = (int) request;
	c.step_active = active;
	cdev->thd_cdev_set_step_request(zone_id, index, c.step_request, active);
}
