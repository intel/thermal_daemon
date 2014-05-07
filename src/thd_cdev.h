/*
 * thd_cdev.h: thermal cooling class interface
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

#ifndef THD_CDEV_H
#define THD_CDEV_H

#include <time.h>
#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_pid.h"

class cthd_cdev {

protected:
	int index;
	csys_fs cdev_sysfs;
	unsigned int trip_point;
	int max_state;
	int min_state;
	int curr_state;
	unsigned long zone_mask;
	unsigned long trip_mask;
	int curr_pow;
	int base_pow_state;
	int inc_dec_val;
	bool auto_down_adjust;
	bool read_back;

	std::string type_str;
	int debounce_interval;
	time_t last_action_time;
	bool trend_increase;
	bool pid_enable;
	cthd_pid pid_ctrl;
	int last_state;

private:
	unsigned int int_2_pow(int pow) {
		int i;
		int _pow = 1;
		for (i = 0; i < pow; ++i)
			_pow = _pow * 2;
		return _pow;
	}
	int thd_cdev_exponential_controller(int set_point, int target_temp,
			int temperature, int state, int arg);

public:
	static const int default_debounce_interval = 3; // In seconds
	cthd_cdev(unsigned int _index, std::string control_path) :
			index(_index), cdev_sysfs(control_path.c_str()), trip_point(0), max_state(
					0), min_state(0), curr_state(0), zone_mask(0), trip_mask(0), curr_pow(
					0), base_pow_state(0), inc_dec_val(1), auto_down_adjust(
					false), read_back(true), debounce_interval(
					default_debounce_interval), last_action_time(0), trend_increase(
					false), pid_enable(false), pid_ctrl(), last_state(0) {
	}

	virtual ~cthd_cdev() {
	}
	virtual int thd_cdev_set_state(int set_point, int target_temp,
			int temperature, int state, int zone_id, int trip_id);

	virtual int thd_cdev_set_min_state(int zone_id);

	virtual void thd_cdev_set_min_state_param(int arg) {
		min_state = arg;
	}
	virtual void thd_cdev_set_max_state_param(int arg) {
		max_state = arg;
	}
	virtual void thd_cdev_set_read_back_param(bool arg) {
		read_back = arg;
	}
	virtual int thd_cdev_get_index() {
		return index;
	}

	virtual int init() {
		return 0;
	}
	;
	virtual int control_begin() {
		if (pid_enable) {
			pid_ctrl.reset();
		}
		return 0;
	}
	;
	virtual int control_end() {
		return 0;
	}
	;
	virtual void set_curr_state(int state, int arg) {
	}
	virtual void set_curr_state_raw(int state, int arg) {
		set_curr_state(state, arg);
	}
	virtual int get_curr_state() {
		return curr_state;
	}

	virtual int get_min_state() {
		return min_state;
	}

	virtual int get_max_state() {
		return max_state;
	}
	;
	virtual int update() {
		return 0;
	}
	;
	virtual void set_inc_dec_value(int value) {
		inc_dec_val = value;
	}
	virtual void set_down_adjust_control(bool value) {
		auto_down_adjust = value;
	}
	void set_debounce_interval(int interval) {
		debounce_interval = interval;
	}
	bool in_min_state() {
		if ((min_state < max_state && get_curr_state() <= min_state)
				|| (min_state > max_state && get_curr_state() >= min_state))
			return true;
		return false;
	}

	bool in_max_state() {
		if ((min_state < max_state && get_curr_state() >= get_max_state())
				|| (min_state > max_state && get_curr_state() <= get_max_state()))
			return true;
		return false;
	}

	std::string get_cdev_type() {
		return type_str;
	}
	std::string get_base_path() {
		return cdev_sysfs.get_base_path();
	}
	void set_cdev_type(std::string _type_str) {
		type_str = _type_str;
	}
	void set_pid_param(double kp, double ki, double kd) {
		pid_ctrl.kp = kp;
		pid_ctrl.ki = ki;
		pid_ctrl.kd = kd;
		thd_log_info("set_pid_param %d [%g.%g,%g]\n", index, kp, ki, kd);
	}
	void enable_pid() {
		thd_log_info("PID control enabled %d\n", index);
		pid_enable = true;
	}

	void cdev_dump() {
		thd_log_info("%d: %s, C:%d MN: %d MX:%d ST:%d pt:%s rd_bk %d \n", index,
				type_str.c_str(), curr_state, min_state, max_state, inc_dec_val,
				get_base_path().c_str(), read_back);
	}
};

#endif
