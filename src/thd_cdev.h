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
#include <vector>
#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_pid.h"
#include "thd_adaptive_types.h"

typedef struct {
	int zone;
	int trip;
	int target_state_valid;
	int target_value;
	int _min_state;
	int _max_state;
	int _min_max_valid;
} zone_trip_limits_t;

#define ZONE_TRIP_LIMIT_COUNT	12

class cthd_cdev {

protected:
	int index;
	csys_fs cdev_sysfs;
	unsigned int trip_point;
	int max_state;
	int min_state;
	int curr_state;
	int curr_pow;
	int base_pow_state;
	int inc_dec_val;
	bool auto_down_adjust;
	bool read_back;

	std::string type_str;
	std::string alias_str;
	int debounce_interval;
	time_t last_action_time;
	bool trend_increase;
	bool pid_enable;
	cthd_pid pid_ctrl;
	int last_state;
	std::vector<zone_trip_limits_t> zone_trip_limits;
	std::string write_prefix;
	int inc_val;
	int dec_val;

private:
	unsigned int int_2_pow(int pow) {
		int i;
		int _pow = 1;
		for (i = 0; i < pow; ++i)
			_pow = _pow * 2;
		return _pow;
	}
	int thd_cdev_exponential_controller(int set_point, int target_temp,
			int temperature, int state, int arg, int temp_min_state = 0,
			int temp_max_state = 0);
	int thd_clamp_state_min(int _state, int temp_min_state = 0);
	int thd_clamp_state_max(int _state, int temp_max_state = 0);
public:
	static const int default_debounce_interval = 2; // In seconds
	cthd_cdev(unsigned int _index, std::string control_path) :
			index(_index), cdev_sysfs(control_path.c_str()), trip_point(0), max_state(
					0), min_state(0), curr_state(0), curr_pow(0), base_pow_state(
					0), inc_dec_val(1), auto_down_adjust(false), read_back(
					true), debounce_interval(default_debounce_interval), last_action_time(
					0), trend_increase(false), pid_enable(false), pid_ctrl(), last_state(
					0), write_prefix(""), inc_val(0), dec_val(0) {
	}

	virtual ~cthd_cdev() {
	}
	virtual int thd_cdev_set_state(int set_point, int target_temp,
			int temperature, int hard_target, int state, int zone_id,
			int trip_id, int target_state_valid, int target_value,
			pid_param_t *pid_param, cthd_pid &pid, bool force,
			int min_max_valid, int _min_state, int _max_state);

	virtual int thd_cdev_set_min_state(int zone_id, int trip_id);

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
		if (state > max_state)
			state = max_state;
		if (state < min_state)
			state = min_state;
		set_curr_state(state, arg);
	}
	virtual int get_curr_state() {
		return curr_state;
	}

	virtual int get_curr_state(bool read_again) {
		return curr_state;
	}

	virtual int get_min_state() {
		return min_state;
	}

	virtual int get_max_state() {
		return max_state;
	}

	virtual int get_phy_max_state() {
		return max_state;
	}

	virtual int update() {
		return 0;
	}
	;
	virtual void set_inc_dec_value(int value) {
		inc_dec_val = value;
	}

	virtual void set_inc_value(int value) {
		inc_val = value;
	}

	virtual void set_dec_value(int value) {
		dec_val = value;
	}

	virtual void set_down_adjust_control(bool value) {
		auto_down_adjust = value;
	}
	virtual int map_target_state(int target_valid, int target_state) {
		return target_state;
	}
	virtual void set_adaptive_target(struct adaptive_target &target) {};
	void set_debounce_interval(int interval) {
		debounce_interval = interval;
	}
	void set_min_state(int _min_state) {
		min_state = _min_state;
	}
	void set_max_state(int _max_state) {
		max_state = _max_state;
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

	int cmp_current_state(int state) {
		if (get_curr_state() == state)
			return 0;

		if (min_state < max_state) {
			if (state > get_curr_state())
				return 1;
			else
				return -1;
		}

		if (min_state > max_state) {
			if (state > get_curr_state())
				return -1;
			else
				return 1;
		}
		return 0;
	}

	std::string get_cdev_type() {
		return type_str;
	}
	std::string get_cdev_alias() {
		return alias_str;
	}
	std::string get_base_path() {
		return cdev_sysfs.get_base_path();
	}
	void set_cdev_type(std::string _type_str) {
		type_str = std::move(_type_str);
	}
	void set_cdev_alias(std::string _alias_str) {
		alias_str = std::move(_alias_str);
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

	void thd_cdev_set_write_prefix(std::string prefix) {
		write_prefix = std::move(prefix);
	}

	void cdev_dump() {
		if (inc_val || dec_val){
			thd_log_info("%d: %s, C:%d MN: %d MX:%d Inc ST:%d Dec ST:%d pt:%s rd_bk %d\n", index,
				type_str.c_str(), curr_state, min_state, max_state, inc_val, dec_val,
				get_base_path().c_str(), read_back);
		} else {
			thd_log_info("%d: %s, C:%d MN: %d MX:%d ST:%d pt:%s rd_bk %d\n", index,
				type_str.c_str(), curr_state, min_state, max_state, inc_dec_val,
				get_base_path().c_str(), read_back);
		}
	}
};

#endif
