/*
 * thd_model.h: thermal model class implementation
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

#include "thd_model.h"
#include <time.h>
#include <math.h>

#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>

cthd_model::cthd_model()
	: trend_increase_start(0),
	  max_temp(def_max_temperature),
	  set_point(def_max_temperature),
	  last_temp(0),
	  max_temp_reached(0),
	  current_angle(0),
	  set_point_reached(false),
	  delay_cnt(0),
	  max_temp_seen(false)
{
}

unsigned int cthd_model::update_set_point()
{

	double slope;
	double delta_x = (max_temp_reached - trend_increase_start) * 1000;
	double delta_y = max_temp -  hot_zone;
	unsigned int _setpoint;
	double radians;
	double arc_len;

	if (delta_y > 0 && delta_x > 0) {
		slope = delta_y /delta_x;
		radians = atan(slope);
		thd_log_info("current slope %g angle before %g (%g degree)\n", slope, radians,  57.2957795 * radians);
		radians += (0.01746 * (current_angle+angle_increment));
		thd_log_info("current slope %g angle before %g (%g degree)\n", slope, radians,  57.2957795 * radians);
		arc_len = delta_x * tan(radians);
		_setpoint = max_temp - (unsigned int)(arc_len - delta_y);
		thd_log_info("** set point  x:%g y:%g arc_len:%g set_point %u\n", delta_x, delta_y, arc_len, _setpoint);
		if (abs(set_point - _setpoint) > max_compensation)
			set_point -= max_compensation;
		else
			set_point = _setpoint;
		current_angle += angle_increment;
	}

	return set_point;
}

void cthd_model::add_sample(unsigned int temperature)
{
	time_t tm;

	time(&tm);
	if (trend_increase_start==0 && temperature > hot_zone) {
		trend_increase_start = tm;
		thd_log_debug("Trend increase start %ld\n",  trend_increase_start);
	} else if (trend_increase_start && temperature < hot_zone) {
		thd_log_debug("Trend increase stopped %ld\n",  trend_increase_start);
		trend_increase_start = 0;
	}
	if (temperature >= max_temp) {
		max_temp_reached = tm;
		// Very first time when we reached max temp
		// then we need to start tuning
		if (!max_temp_seen) {
			unsigned int _set_point;
			update_set_point();
			_set_point = read_set_point();
			// Update only if the current set point is more than
			// the stored one.
			if (_set_point == 0 || set_point > _set_point)
				store_set_point();
			max_temp_seen = true;
		}
		// Give some time to cooling device to cool, after that set next set point
		if (delay_cnt > def_setpoint_delay_cnt && (last_temp < temperature))
			update_set_point();
		delay_cnt++;
		set_point_reached = true;
	} else {
		set_point_reached = false;
		delay_cnt = 0;
	}
	last_temp = temperature;
	thd_log_info("update_set_point %u,%d,%u\n", last_temp, current_angle, set_point);
}

void cthd_model::store_set_point()
{
	std::stringstream filename;
	std::ofstream file;

	filename << TDCONFDIR << "/" << "thermal_set_point.conf";
	std::ofstream fout(filename.str().c_str());
	if (fout.good()){
        fout << set_point;
	}
	thd_log_info("storing set point %u\n", set_point);
	fout.close();
}

unsigned int cthd_model::read_set_point()
{
	std::stringstream filename;
	unsigned int _set_point = 0;

	filename << TDCONFDIR << "/" << "thermal_set_point.conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (ifs.good()){
		ifs >> _set_point;
	}
	ifs.close();
	thd_log_info("Read set point %u\n", _set_point);
	return _set_point;
}

void cthd_model::set_max_temperature(unsigned int temp)
{
	unsigned int _set_point;

	max_temp = temp - safety_margin;
	_set_point = read_set_point();
	if ( _set_point > 0) {
		set_point = _set_point;
	} else {
		set_point = max_temp;
	}
	hot_zone = max_temp - ((max_temp*hot_zone_percent)/100);
}
