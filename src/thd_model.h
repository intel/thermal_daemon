/*
 * thd_model.h: thermal model class interface
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

class cthd_model {

private:
	static const int def_max_temperature = 100*1000;
	static const int mov_average_window = 5;
	static const int trend_increase_offset = 1000;
	static const unsigned int safety_margin = 3*1000;
	static const int def_low_angle_from_max = 5;
	static const int angle_increment = 2;
	static const int def_setpoint_delay_cnt = 8;
	static const unsigned int max_compensation = 5000;

	void add(unsigned int value)
	{
		if (num_samples < mov_average_window) {
			samples[num_samples++] = value;
			current_total += value;
		} else {
			unsigned int& oldest = samples[num_samples++ % mov_average_window];
			current_total += value - oldest;
			oldest = value;
		}
	}

	unsigned int moving_average(unsigned int value)
	{
		add(value);
		return current_total / std::min(num_samples, mov_average_window);
	}

	unsigned int samples[mov_average_window];
	int num_samples;
	unsigned int current_total;

	unsigned int max_temp;
	unsigned int last_temp;

	time_t	start_time;
	time_t trend_increase_start;
	time_t trend_decrease_start;

	time_t max_temp_reached;

	unsigned int set_point;
	unsigned int set_point_next;

	int	trend_increase;
	int begin_moving_average;
	int current_moving_average;
	int last_moving_average;

	int current_angle;
	bool set_point_reached;
	int delay_cnt;
	bool max_temp_seen;

	unsigned int update_set_point();

	void store_set_point();
	unsigned int read_set_point();

public:
	cthd_model();

	void add_sample(unsigned int temperature);
	void set_max_temperature(unsigned int temp);
	unsigned int get_set_point() { return set_point; }
	bool is_set_point_reached() { return set_point_reached; };
	unsigned int get_moving_average() { return current_moving_average; }
};
