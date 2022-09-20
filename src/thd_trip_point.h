/*
 * thd_trip_point.h: thermal zone trip points class interface
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

#ifndef THD_TRIP_POINT_H
#define THD_TRIP_POINT_H

#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_cdev.h"

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include <time.h>
#include <vector>
#include <algorithm>    // std::sort
#include <stdexcept>

typedef enum {
	CRITICAL, HOT, MAX, PASSIVE, ACTIVE, POLLING, INVALID_TRIP_TYPE
} trip_point_type_t;

typedef enum {
	PARALLEL,  // All associated cdevs are activated together
	SEQUENTIAL  // one after other once the previous cdev reaches its max state
} trip_control_type_t;

#define TRIP_PT_INVALID_TARGET_STATE	INT32_MAX

typedef enum {
	EQUAL, GREATER, LESSER, LESSER_OR_EQUAL, GREATER_OR_EQUAL
} trip_point_cdev_depend_rel_t;

typedef struct {
	cthd_cdev *cdev;
	int influence;
	int sampling_priod;
	time_t last_op_time;
	int target_state_valid;
	int target_state;
	pid_param_t pid_param;
	cthd_pid pid;
	int min_max_valid;
	int min_state;
	int max_state;
} trip_pt_cdev_t;

#define DEFAULT_SENSOR_ID	0xFFFF

static bool trip_cdev_sort(trip_pt_cdev_t &cdev1, trip_pt_cdev_t &cdev2) {
	return (cdev1.influence > cdev2.influence);
}


class cthd_trip_point {
private:
	int index;
	trip_point_type_t type;
	unsigned int temp;
	unsigned int hyst;
	std::vector<trip_pt_cdev_t> cdevs;
	trip_control_type_t control_type;
	int zone_id;
	int sensor_id;
	bool trip_on;
	bool poll_on;

	cthd_cdev *depend_cdev;
	int depend_cdev_state;
	trip_point_cdev_depend_rel_t depend_cdev_state_rel;
	int crit_trip_count;

	bool check_duplicate(cthd_cdev *cdev, int *index) {
		for (unsigned int i = 0; i < cdevs.size(); ++i) {
			if (cdevs[i].cdev->get_cdev_type() == cdev->get_cdev_type()) {
				*index = i;
				return true;
			}
		}
		return false;
	}

public:
	static const int default_influence = 0;
	static const int consecutive_critical_events = 4;

	cthd_trip_point(int _index, trip_point_type_t _type, unsigned int _temp,
			unsigned int _hyst, int _zone_id, int _sensor_id,
			trip_control_type_t _control_type = PARALLEL);
	bool thd_trip_point_check(int id, unsigned int read_temp, int pref,
			bool *reset);

	void thd_trip_point_add_cdev(cthd_cdev &cdev, int influence,
			int sampling_period = 0, int target_state_valid = 0,
			int target_state =
			TRIP_PT_INVALID_TARGET_STATE, pid_param_t *pid_param = NULL,
			int min_max_valid = 0, int min_state = 0, int max_state = 0);

	void delete_cdevs() {
		cdevs.clear();
	}

	void thd_trip_cdev_state_reset(int force = 0);
	int thd_trip_point_value() {
		return temp;
	}
	void thd_trip_update_set_point(unsigned int new_value) {
		temp = new_value;
	}
	int thd_trip_point_add_cdev_index(int _index, int influence);
	void thd_trip_point_set_control_type(trip_control_type_t type) {
		control_type = type;
	}
	trip_point_type_t get_trip_type() {
		return type;
	}
	unsigned int get_trip_temp() {
		return temp;
	}
	unsigned int get_trip_hyst() {
		return hyst;
	}
	void update_trip_temp(unsigned int _temp) {
		temp = _temp;
	}
	void update_trip_type(trip_point_type_t _type) {
		type = _type;
	}
	void update_trip_hyst(unsigned int _temp) {
		hyst = _temp;
	}
	int get_sensor_id() {
		return sensor_id;
	}
	unsigned int get_cdev_count() {
		return cdevs.size();
	}


	int is_target_valid(int &target_state) {
		target_state = 0;
		for (unsigned int i = 0; i < cdevs.size(); ++i) {
			trip_pt_cdev_t &cdev = cdevs[i];

			if (cdev.target_state_valid) {
				thd_log_debug("matched %d\n", cdev.target_state);
				target_state = cdev.target_state;
				return THD_SUCCESS;
			}
		}

		return THD_ERROR;
	}

	int set_first_target_invalid() {
		if (cdevs.size()) {
			trip_pt_cdev_t &cdev = cdevs[0];
			cdev.target_state_valid = 0;
			return THD_SUCCESS;
		}

		return THD_ERROR;
	}

	int set_first_target(int state) {
		if (cdevs.size()) {
			trip_pt_cdev_t &cdev = cdevs[0];
			cdev.target_state_valid = 1;
			cdev.target_state = state;
			return THD_SUCCESS;
		}

		return THD_ERROR;
	}

	cthd_cdev* get_first_cdev() {
		if (!cdevs.size())
			return NULL;

		return cdevs[0].cdev;
	}

	void set_dependency(std::string cdev, std::string state_str);

#ifndef ANDROID
	trip_pt_cdev_t &get_cdev_at_index(unsigned int index) {
		if (index < cdevs.size())
			return cdevs[index];
		else
			throw std::invalid_argument("index");
	}
#endif

	void trip_cdev_add(trip_pt_cdev_t &trip_cdev) {
		int index;
		if (check_duplicate(trip_cdev.cdev, &index)) {
			cdevs[index].influence = trip_cdev.influence;
		} else
			cdevs.push_back(trip_cdev);

		std::sort(cdevs.begin(), cdevs.end(), trip_cdev_sort);
	}
	void trip_dump() {
		std::string _type_str;
		if (type == CRITICAL)
			_type_str = "critical";
		else if (type == MAX)
			_type_str = "max";
		else if (type == PASSIVE)
			_type_str = "passive";
		else if (type == ACTIVE)
			_type_str = "active";
		else if (type == POLLING)
			_type_str = "polling";
		else if (type == HOT)
			_type_str = "hot";
		else
			_type_str = "invalid";
		thd_log_info(
				"index %d: type:%s temp:%u hyst:%u zone id:%d sensor id:%d control_type:%d cdev size:%lu\n",
				index, _type_str.c_str(), temp, hyst, zone_id, sensor_id,
				control_type, (unsigned long) cdevs.size());

		if (depend_cdev) {
			thd_log_info("Depends on cdev %s:%d:%d\n",
					depend_cdev->get_cdev_type().c_str(), depend_cdev_state_rel,
					depend_cdev_state);
		}

		for (unsigned int i = 0; i < cdevs.size(); ++i) {
			thd_log_info("cdev[%u] %s, Sampling period: %d\n", i,
					cdevs[i].cdev->get_cdev_type().c_str(),
					cdevs[i].sampling_priod);
			if (cdevs[i].target_state_valid)
				thd_log_info("\t target_state:%d\n", cdevs[i].target_state);
			else
				thd_log_info("\t target_state:not defined\n");

			thd_log_info("min_max %d\n", cdevs[i].min_max_valid);

			if (cdevs[i].pid_param.valid)
				thd_log_info("\t pid: kp=%g ki=%g kd=%g\n",
						cdevs[i].pid_param.kp, cdevs[i].pid_param.ki,
						cdevs[i].pid_param.kd);
			if (cdevs[i].min_max_valid) {
				thd_log_info("\t min_state:%d\n", cdevs[i].min_state);
				thd_log_info("\t max_state:%d\n", cdevs[i].max_state);
			}
		}
	}
};

static inline bool trip_sort(cthd_trip_point trip1, cthd_trip_point trip2) {
	return (trip1.get_trip_temp() < trip2.get_trip_temp());
}
#endif
