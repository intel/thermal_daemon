/*
 * thd_trip_point.cpp: thermal zone class implentation
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

#include <unistd.h>
#include <sys/reboot.h>


#include "thd_trip_point.h"
#include "thd_engine.h"
#include "thd_engine_custom.h"


cthd_trip_point::cthd_trip_point(int _index, trip_point_type_t _type, unsigned int _temp, unsigned int _hyst)
		: index(_index), type(_type), temp(_temp), hyst(_hyst), pid_enable(false) {
	thd_log_debug("Add trip pt %d:%d:%d\n", type, temp, hyst);
}

bool cthd_trip_point::thd_trip_point_check(unsigned int read_temp, int pref)
{
	int on = -1;
	int off = -1;
	int pid_state = -1;

	if (pid_enable) {
		pid_controller.print_pid_constants();
	}
	if (read_temp == 0) {
		thd_log_warn("TEMP == 0 pref: %d\n", pref);
	}

	if (type == CRITICAL) {
		if (read_temp >= temp) {
			thd_log_warn("critical temp reached \n");
			sync();
			reboot(RB_POWER_OFF);
		}
	}
	if (pref == PREF_DISABLED)
		return FALSE;

	if (pref == PREF_PERFORMANCE && type == ACTIVE) {
		if (read_temp >= temp) {
			thd_log_debug("Active Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
		}
		else if ((read_temp - hyst) < temp) {
			thd_log_debug("Active Trip point applicable =<  %d:%d \n", index, temp);
			off = 1;
		}
	}

	if (pref == PREF_ENERGY_CONSERVE && type == PASSIVE) { // ?? Active trip point will still be applicable?
		if (read_temp >= temp) {
			if (pid_enable) {
				//thd_log_debug("PID controller address >  %x\n",pid_controller);
				pid_state = pid_controller.update_pid(read_temp, temp);
			}
			thd_log_debug("passive Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
		}
		else if ((read_temp - hyst) < temp){
			thd_log_debug("passive Trip point applicable =<  %d:%d \n", index, temp);
			off = 1;
		}
	}

	thd_log_debug("cdev_index for this trip pt %d\n", cdev_indexes.size());
	int i;
	for (i=0; i<cdev_indexes.size(); ++i) {
		cthd_cdev cdev = thd_engine.thd_get_cdev_at_index(cdev_indexes[i]);
		thd_log_debug("cdev at index %d\n", cdev.thd_cdev_get_index());

		if (on > 0)
			cdev.thd_cdev_set_state(temp, read_temp, 1);
		if (off > 0)
			cdev.thd_cdev_set_state(temp, read_temp, 0);
	}
	return TRUE;
}

void cthd_trip_point::thd_trip_point_add_cdev_index(int _index)
{
	thd_log_debug("thd_trip_point_add_cdev_index trip_index %d cdev index %d\n", index, _index);
	cdev_indexes.push_back(_index);
}

void cthd_trip_point::thd_set_pid(cthd_pid &pid_ctrl)
{
	pid_controller = pid_ctrl;
	pid_enable = true;
	pid_controller.print_pid_constants();
}
