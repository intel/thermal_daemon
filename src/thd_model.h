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

#include "thermald.h"
#include <vector>
#include <time.h>

class cthd_model
{

private:
	static const int def_max_temperature = 100 * 1000;
	static const unsigned int safety_margin = 1*1000;
	static const int angle_increment = 1;
	static const int def_setpoint_delay_cnt = 3;
	static const unsigned int max_compensation = 5000;
	static const unsigned int hot_zone_percent = 20; //20%
	static const time_t set_point_delay_tm	= 4 * 3; // 12 seconds

	unsigned int max_temp;
	unsigned int hot_zone;
	unsigned int last_temp;

	time_t trend_increase_start;
	time_t trend_decrease_start;
	time_t max_temp_reached;
	time_t set_point_delay_start;

	unsigned int set_point;

	int current_angle;
	int delay_cnt;
	bool max_temp_seen;
	bool set_point_reached;
	bool updated_set_point;
	bool user_forced_set_point_change;

	unsigned int update_set_point(unsigned int curr_temp);
	void store_set_point();
	unsigned int read_set_point();
	unsigned int read_user_set_max_temp();

	double kp, ki, kd, err_sum, last_err;
	time_t last_time;
	bool use_pid_param;

public:
	cthd_model(bool use_pid=false);

	void add_sample(unsigned int temperature);
	void set_max_temperature(unsigned int temp);
	bool update_user_set_max_temp();

	unsigned int get_set_point()
	{
		return set_point;
	}

	unsigned int get_hot_zone_trigger_point()
	{
		return hot_zone;
	}

	bool is_set_point_reached()
	{
		return (set_point_reached || updated_set_point);
	};
};
