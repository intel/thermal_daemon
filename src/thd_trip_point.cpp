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

#include "thd_trip_point.h"
#include "thd_engine.h"
#include "thd_engine_custom.h"

cthd_trip_point::cthd_trip_point(int _index, trip_point_type_t _type, unsigned int _temp, unsigned int _hyst)
		: index(_index), type(_type), temp(_temp), hyst(_hyst) {
	thd_log_debug("Add trip pt %d:%d:%d\n", type, temp, hyst);
}

bool cthd_trip_point::thd_trip_point_check(unsigned int read_temp)
{
	cthd_preference thd_pref;
	int pref = thd_pref.get_preference();
	int on = -1;
	int off = -1;

	if (pref == PREF_DISABLED)
		return FALSE;

	if (pref == PREF_PERFORMANCE && type == ACTIVE) {
		if (read_temp >= temp) {
			thd_log_debug("Active Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
		}
		else {
			thd_log_debug("Active Trip point applicable =<  %d:%d \n", index, temp);
			off = 1;
		}
	}

	if (pref == PREF_ENERGY_CONSERVE && type == PASSIVE) { // ?? Active trip point will still be applicable?
		if (read_temp >= temp) {
			thd_log_debug("passive Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
		}
		else {
			thd_log_debug("passive Trip point applicable =<  %d:%d \n", index, temp);
			off = 1;
		}
	}

	thd_log_debug("cdev_index for this trip pt %d\n", cdev_indexes.size());
	int i;
	for (i=0; i<cdev_indexes.size(); ++i) {
		//cthd_cdev cdev;
		// Get cdev indexes
		cthd_cdev cdev = thd_engine.thd_get_cdev_at_index(cdev_indexes[i]);
//		if (!cdev)
//			continue;
		thd_log_debug("cdev at index %d\n", cdev.thd_cdev_get_index());

		if (on > 0)
			cdev.thd_cdev_set_state(1);
		if (off > 0)
			cdev.thd_cdev_set_state(0);
	}
	return TRUE;
}

void cthd_trip_point::thd_trip_point_add_cdev_index(int _index)
{
	thd_log_debug("thd_trip_point_add_cdev_index trip_index %d cdev index %d\n", index, _index);
	cdev_indexes.push_back(_index);
}
