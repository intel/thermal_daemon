/*
 * thd_trip_point.cpp: thermal zone class implentation
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
#include <sys/reboot.h>
#include "thd_trip_point.h"
#include "thd_engine.h"

cthd_trip_point::cthd_trip_point(int _index, trip_point_type_t _type, unsigned
int _temp, unsigned int _hyst, int _zone_id, int _sensor_id,
		trip_control_type_t _control_type) :
		index(_index), type(_type), temp(_temp), hyst(_hyst), control_type(
				_control_type), zone_id(_zone_id), sensor_id(_sensor_id), trip_on(
				false), poll_on(false) {
	thd_log_debug("Add trip pt %d:%d:0x%x:%d:%d\n", type, zone_id, sensor_id,
			temp, hyst);
}

bool cthd_trip_point::thd_trip_point_check(int id, unsigned int read_temp,
		int pref, bool *reset) {
	int on = -1;
	int off = -1;
	bool apply = false;

	*reset = false;

	if (sensor_id != DEFAULT_SENSOR_ID && sensor_id != id)
		return false;

	if (read_temp == 0) {
		thd_log_debug("TEMP == 0 pref: %d\n", pref);
	}

	if (type == CRITICAL) {
		if (read_temp >= temp) {
			thd_log_warn("critical temp reached \n");
			sync();
			reboot(RB_POWER_OFF);
		}
	}

	if (type == POLLING && sensor_id != DEFAULT_SENSOR_ID) {
		cthd_sensor *sensor = thd_engine->get_sensor(sensor_id);
		if (sensor && sensor->check_async_capable()) {
			if (!poll_on && read_temp >= temp) {
				thd_log_debug("polling trip reached, on \n");
				sensor->sensor_poll_trip(true);
				poll_on = true;
				sensor->set_threshold(0, temp);
			} else if (poll_on && read_temp < temp) {
				thd_log_debug("polling trip reached, off \n");
				sensor->sensor_poll_trip(false);
				thd_log_info("Dropped below poll threshold \n");
				*reset = true;
				poll_on = false;
				sensor->set_threshold(0, temp);
			}
		}
		return true;
	}
	thd_log_debug("pref %d type %d temp %d trip %d \n", pref, type, read_temp,
			temp);
	switch (pref) {
	case PREF_DISABLED:
		return false;
		break;

	case PREF_PERFORMANCE:
		if (type == ACTIVE || type == MAX) {
			apply = true;
			thd_log_debug("Active Trip point applicable \n");
		}
		break;

	case PREF_ENERGY_CONSERVE:
		if (type == PASSIVE || type == MAX) {
			apply = true;
			thd_log_debug("Passive Trip point applicable \n");
		}
		break;

	default:
		break;
	}

	if (apply) {
		if (read_temp >= temp) {
			thd_log_debug("Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
			trip_on = true;
		} else if ((trip_on && (read_temp + hyst) < temp)
				|| (!trip_on && read_temp < temp)) {
			thd_log_debug("Trip point applicable <  %d:%d \n", index, temp);
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
					thd_log_info("Too early to act index %d tm %ld\n",
							cdev->thd_cdev_get_index(),
							tm - cdevs[i].last_op_time);
					break;
				}
				cdevs[i].last_op_time = tm;
			}
			thd_log_debug("cdev at index %d:%s\n", cdev->thd_cdev_get_index(),
					cdev->get_cdev_type().c_str());
			if (cdev->in_max_state()) {
				thd_log_debug("Need to switch to next cdev \n");
				// No scope of control with this cdev
				continue;
			}
			ret = cdev->thd_cdev_set_state(temp, temp, read_temp, 1, zone_id,
					index);
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
				thd_log_debug("Need to switch to next cdev \n");
				// No scope of control with this cdev
				continue;
			}
			cdev->thd_cdev_set_state(temp, temp, read_temp, 0, zone_id, index);
			if (control_type == SEQUENTIAL) {
				// Only one cdev activation
				break;
			}
		}
	}

	return true;
}

void cthd_trip_point::thd_trip_point_add_cdev(cthd_cdev &cdev, int influence,
		int sampling_period) {
	trip_pt_cdev_t thd_cdev;
	thd_cdev.cdev = &cdev;
	thd_cdev.influence = influence;
	thd_cdev.sampling_priod = sampling_period;
	thd_cdev.last_op_time = 0;
	trip_cdev_add(thd_cdev);
}

int cthd_trip_point::thd_trip_point_add_cdev_index(int _index, int influence) {
	cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(_index);
	if (cdev) {
		trip_pt_cdev_t thd_cdev;
		thd_cdev.cdev = cdev;
		thd_cdev.influence = influence;
		thd_cdev.sampling_priod = 0;
		thd_cdev.last_op_time = 0;
		trip_cdev_add(thd_cdev);
		return THD_SUCCESS;
	} else {
		thd_log_warn("thd_trip_point_add_cdev_index not present %d\n", _index);
		return THD_ERROR;
	}
}

void cthd_trip_point::thd_trip_cdev_state_reset() {
	thd_log_info("thd_trip_cdev_state_reset \n");
	for (int i = cdevs.size() - 1; i >= 0; --i) {
		cthd_cdev *cdev = cdevs[i].cdev;
		thd_log_info("thd_trip_cdev_state_reset index %d:%s\n",
				cdev->thd_cdev_get_index(), cdev->get_cdev_type().c_str());
		if (cdev->in_min_state()) {
			thd_log_debug("Need to switch to next cdev \n");
			// No scope of control with this cdev
			continue;
		}
		cdev->thd_cdev_set_min_state(zone_id);
	}
}
