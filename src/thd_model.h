/*
 * thd_model.h: thermal model class interface
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

#ifndef _THD_MODEL_H
#define _THD_MODEL_H

#include "thermald.h"
#include <vector>
#include <time.h>
#include <string>

class cthd_model {

private:
	static const int def_max_temperature = 100 * 1000;
	static const unsigned int safety_margin = 1 * 1000;
	static const int angle_increment = 1;
	static const int def_setpoint_delay_cnt = 3;
	static const int max_compensation = 5000;
	static const unsigned int hot_zone_percent = 20; //20%
	static const time_t set_point_delay_tm = 4 * 3; // 12 seconds

	std::string zone_type;
	time_t trend_increase_start;
	int max_temp;
	int set_point;

	int hot_zone;
	int last_temp;

	time_t trend_decrease_start;
	time_t max_temp_reached;

	int current_angle;
	bool set_point_reached;
	int delay_cnt;
	bool max_temp_seen;
	bool updated_set_point;
	bool use_pid_param;
	time_t set_point_delay_start;
	bool user_forced_set_point_change;

	unsigned int update_set_point(unsigned int curr_temp);
	void store_set_point();
	int read_set_point();
	int read_user_set_max_temp();

	double kp, ki, kd, err_sum, last_err;
	time_t last_time;

public:
	cthd_model(std::string _zone_type, bool use_pid = false);

	void add_sample(int temperature);
	void set_max_temperature(int temp);
	bool update_user_set_max_temp();

	void set_zone_type(std::string _zone_type) {
		zone_type = _zone_type;
	}

	void use_pid() {
		use_pid_param = true;
	}

	unsigned int get_set_point() {
		return set_point;
	}

	unsigned int get_hot_zone_trigger_point() {
		return hot_zone;
	}

	bool is_set_point_reached() {
		return (set_point_reached || updated_set_point);
	}
	;
};

#endif
