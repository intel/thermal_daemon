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
#include <time.h>
#include <vector>
#include <algorithm>    // std::sort
typedef enum {
	CRITICAL, MAX, PASSIVE, ACTIVE, POLLING, INVALID_TRIP_TYPE
} trip_point_type_t;

typedef enum {
	PARALLEL,  // All associated cdevs are activated together
	SEQUENTIAL  // one after other once the previous cdev reaches its max state
} trip_control_type_t;

typedef struct {
	cthd_cdev *cdev;
	int influence;
	int sampling_priod;
	time_t last_op_time;
} trip_pt_cdev_t;

#define DEFAULT_SENSOR_ID	0xFFFF

static bool trip_cdev_sort(trip_pt_cdev_t cdev1, trip_pt_cdev_t cdev2) {
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
	cthd_trip_point(int _index, trip_point_type_t _type, unsigned int _temp,
			unsigned int _hyst, int _zone_id, int _sensor_id,
			trip_control_type_t _control_type = PARALLEL);
	bool thd_trip_point_check(int id, unsigned int read_temp, int pref,
			bool *reset);

	void thd_trip_point_add_cdev(cthd_cdev &cdev, int influence,
			int sampling_period = 0);

	void thd_trip_cdev_state_reset();
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
	void trip_cdev_add(trip_pt_cdev_t trip_cdev) {
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
		else
			_type_str = "invalid";
		thd_log_info(
				"index %d: type:%s temp:%u hyst:%u zone id:%d sensor id:%d cdev size:%lu\n",
				index, _type_str.c_str(), temp, hyst, zone_id, sensor_id,
				(unsigned long) cdevs.size());
		for (unsigned int i = 0; i < cdevs.size(); ++i) {
			thd_log_info("cdev[%i] %s\n", i,
					cdevs[i].cdev->get_cdev_type().c_str());
		}
	}
};

static bool trip_sort(cthd_trip_point trip1, cthd_trip_point trip2) {
	return (trip1.get_trip_temp() < trip2.get_trip_temp());
}
#endif
