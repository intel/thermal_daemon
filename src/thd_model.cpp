/*
 * thd_model.h: thermal model class implementation
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

#include "thd_model.h"
#include "thd_engine.h"
#include "thd_zone.h"

#include <time.h>
#include <math.h>

#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>

/*
 * Dynamically adjust set point based on the amount of time spent on hot zone.
 */
cthd_model::cthd_model(std::string _zone_type, bool use_pid) :
		zone_type(_zone_type), trend_increase_start(0), max_temp(
				def_max_temperature), set_point(def_max_temperature), hot_zone(
				0), last_temp(0), trend_decrease_start(0), max_temp_reached(0), current_angle(
				0), set_point_reached(false), delay_cnt(0), max_temp_seen(
				false), updated_set_point(false), use_pid_param(use_pid), set_point_delay_start(
				0), user_forced_set_point_change(false) {
	// set default pid parameters
	kp = 0.5;
	ki = kd = 0.0001;
	last_time = 0;
	err_sum = 0.0;
	last_err = 0.0;
}

unsigned int cthd_model::update_set_point(unsigned int curr_temp) {
	if (!use_pid_param) {
		double slope;
		double delta_x = (max_temp_reached - trend_increase_start) * 1000;
		double delta_y = max_temp - hot_zone;
		int _setpoint;
		double radians;
		double arc_len;

		if (delta_y > 0 && delta_x > 0) {
			slope = delta_y / delta_x;
			radians = atan(slope);
			thd_log_info("current slope %g angle before %g (%g degree)\n",
					slope, radians, 57.2957795 * radians);
			radians += (0.01746 * (current_angle + angle_increment));
			thd_log_info("current slope %g angle before %g (%g degree)\n",
					slope, radians, 57.2957795 * radians);
			arc_len = delta_x * tan(radians);
			_setpoint = max_temp - (unsigned int) (arc_len - delta_y);
			_setpoint = _setpoint - _setpoint % 1000;
			thd_log_info("** set point  x:%g y:%g arc_len:%g set_point %d\n",
					delta_x, delta_y, arc_len, _setpoint);
			if ((_setpoint < 0)
					|| (abs(set_point - _setpoint) > max_compensation))
				set_point -= max_compensation;
			else
				set_point = _setpoint;

			if (set_point < hot_zone)
				set_point = hot_zone;

			current_angle += angle_increment;
		}

		return set_point;
	} else {
		double output;
		double d_err = 0;
		int _setpoint;

		time_t now;
		time(&now);
		if (last_time == 0)
			last_time = now;
		time_t timeChange = (now - last_time);

		int error = curr_temp - max_temp;
		err_sum += (error * timeChange);
		if (timeChange)
			d_err = (error - last_err) / timeChange;
		else
			d_err = 0.0;

		/*Compute PID Output*/
		output = kp * error + ki * err_sum + kd * d_err;
		_setpoint = max_temp - (unsigned int) output;
		thd_log_info("update_pid %ld %ld %d %g %d\n", now, last_time, error,
				output, _setpoint);
		if ((_setpoint < 0) || (abs(set_point - _setpoint) > max_compensation))
			set_point -= max_compensation;
		else
			set_point = _setpoint;

		if (set_point < hot_zone)
			set_point = hot_zone;

		/*Remember some variables for next time*/
		last_err = error;
		last_time = now;

		return set_point;
	}
}

void cthd_model::add_sample(int temperature) {
	time_t tm;

	time(&tm);
	updated_set_point = false;
	if (trend_increase_start == 0 && temperature > hot_zone) {
		trend_increase_start = tm;
		thd_log_debug("Trend increase start %ld\n", trend_increase_start);
	} else if (trend_increase_start && temperature < hot_zone) {
		int _set_point;
		thd_log_debug("Trend increase stopped %ld\n", trend_increase_start);
		trend_increase_start = 0;
		_set_point = read_set_point(); // Restore set point to a calculated max
		if (_set_point > set_point) {
			set_point = _set_point;
			updated_set_point = true;
			current_angle = 0;
			// Reset PID params
			err_sum = last_err = 0.0;
			last_time = 0;
		}
	}
	if (temperature > max_temp) {
		max_temp_reached = tm;
		// Very first time when we reached max temp
		// then we need to start tuning
		if (!max_temp_seen) {
			int _set_point;
			update_set_point(temperature);
			_set_point = read_set_point();
			// Update only if the current set point is more than
			// the stored one.
			if (_set_point == 0 || set_point > _set_point)
				store_set_point();
			max_temp_seen = true;
		}
		// Give some time to cooling device to cool, after that set next set point
		if (set_point_delay_start
				&& (tm - set_point_delay_start) >= set_point_delay_tm
				&& (last_temp < temperature))
			update_set_point(temperature);
		delay_cnt++;
		if (!set_point_delay_start)
			set_point_delay_start = tm;
		set_point_reached = true;
	} else {
		set_point_reached = false;
		delay_cnt = 0;
		set_point_delay_start = 0;
	}
	if (user_forced_set_point_change) {
		user_forced_set_point_change = false;
		set_point_reached = true;
	}
	last_temp = temperature;
	thd_log_debug("update_set_point %u,%d,%u\n", last_temp, current_angle,
			set_point);
}

void cthd_model::store_set_point() {
	std::stringstream filename;

	filename << TDRUNDIR << "/" << "thermal_set_point." << zone_type << "."
			<< "conf";
	std::ofstream fout(filename.str().c_str());
	if (fout.good()) {
		fout << set_point;
	}
	thd_log_info("storing set point %d\n", set_point);
	fout.close();
}

int cthd_model::read_set_point() {
	std::stringstream filename;
	unsigned int _set_point = 0;

	filename << TDRUNDIR << "/" << "thermal_set_point." << zone_type << "."
			<< "conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (ifs.good()) {
		ifs >> _set_point;
	}
	ifs.close();
	thd_log_info("Read set point %u\n", _set_point);

	return _set_point;
}

void cthd_model::set_max_temperature(int temp) {
	int _set_point;
	int user_defined_max;

	max_temp = temp - safety_margin;
	user_defined_max = read_user_set_max_temp();
	if (user_defined_max > 0)
		max_temp = user_defined_max;
	_set_point = read_set_point();
	if (_set_point > 0 && _set_point < max_temp) {
		set_point = _set_point;
	} else {
		set_point = max_temp;
	}
	hot_zone = max_temp - ((max_temp * hot_zone_percent) / 100);
}

bool cthd_model::update_user_set_max_temp() {
	std::stringstream filename;
	bool present = false;
	unsigned int temp;

	filename << TDRUNDIR << "/" << "thd_user_set_max." << zone_type << "."
			<< "conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (ifs.good()) {
		ifs >> temp;
		if (temp > 1000) {
			max_temp = temp;
			set_point = max_temp;
			hot_zone = max_temp - ((max_temp * hot_zone_percent) / 100);
			store_set_point();
			present = true;
			user_forced_set_point_change = true;
			thd_log_info("User forced maximum temperature is %d\n", max_temp);
		}
	}
	ifs.close();

	return present;
}

int cthd_model::read_user_set_max_temp() {
	std::stringstream filename;
	unsigned int user_max = 0;

	filename << TDRUNDIR << "/" << "thd_user_set_max." << zone_type << "."
			<< "conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (ifs.good()) {
		ifs >> user_max;
		thd_log_info("User defined max temperature %u\n", user_max);
	}
	ifs.close();

	return user_max;
}
