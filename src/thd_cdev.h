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

#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include <time.h>

class cthd_cdev
{

protected:
	int index;
	csys_fs cdev_sysfs;
	unsigned int trip_point;
	int max_state;
	int min_state;
	int curr_state;
	unsigned int sensor_mask;
	time_t last_op_time;
	int curr_pow;
	int base_pow_state;
	int inc_dec_val;
	bool auto_down_adjust;

	std::string type_str;

private:
	unsigned int int_2_pow(int pow)
	{
		int i;
		int _pow = 1;
		for(i = 0; i < pow; ++i)
			_pow = _pow * 2;
		return _pow;
	}

public:
	cthd_cdev(unsigned int _index, std::string control_path): index(_index),
	cdev_sysfs(control_path.c_str()), trip_point(0), max_state(0), min_state(0),
	curr_state(0), sensor_mask(0), last_op_time(0), curr_pow(0), base_pow_state
	(0), inc_dec_val(1), auto_down_adjust(false){}

	virtual int thd_cdev_set_state(int set_point, int temperature, int state, int arg);

	virtual int thd_cdev_get_index()
	{
		return index;
	}

	virtual int init()
	{
		return 0;
	};
	virtual int control_begin()
	{
		return 0;
	};
	virtual int control_end()
	{
		return 0;
	};
	virtual void set_curr_state(int state, int arg){}

	virtual int get_curr_state()
	{
		return curr_state;
	}

	virtual int get_max_state()
	{
		return 0;
	};
	virtual int update()
	{
		return 0;
	};
	virtual void set_inc_dec_value(int value)
	{
		inc_dec_val = value;
	}
	virtual void set_down_adjust_control(bool value)
	{
		auto_down_adjust = value;
	}
	bool in_min_state()
	{
		if(get_curr_state() <= min_state)
			return true;
		return false;
	}

	bool in_max_state()
	{
		if(get_curr_state() >= get_max_state())
			return true;
		return false;
	}

};

#endif
