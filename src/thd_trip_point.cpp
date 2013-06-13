/*
 * thd_trip_point.cpp: thermal zone class implentation
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

#include <unistd.h>
#include <sys/reboot.h>
#include "thd_trip_point.h"
#include "thd_engine.h"

cthd_trip_point::cthd_trip_point(int _index, trip_point_type_t _type, unsigned
	int _temp, unsigned int _hyst, int _arg): index(_index), type(_type), temp
	(_temp), hyst(_hyst), control_type(PARALLEL), arg(_arg)
{
	thd_log_debug("Add trip pt %d:%d:%d\n", type, temp, hyst);
}

bool cthd_trip_point::thd_trip_point_check(unsigned int read_temp, int pref)
{
	int on =  - 1;
	int off =  - 1;
	int pid_state =  - 1;
	bool apply = false;

	if(read_temp == 0)
	{
		thd_log_warn("TEMP == 0 pref: %d\n", pref);
	}

	if(type == CRITICAL)
	{
		if(read_temp >= temp)
		{
			thd_log_warn("critical temp reached \n");
			sync();
			reboot(RB_POWER_OFF);
		}
	}

	thd_log_debug("pref %d type %d\n", pref, type);
	switch(pref)
	{
		case PREF_DISABLED:
			return FALSE;
			break;

		case PREF_PERFORMANCE:
			if(type == ACTIVE)
			{
				apply = true;
				thd_log_debug("Active Trip point applicable \n");
			}
			break;

		case PREF_ENERGY_CONSERVE:
			if(type == PASSIVE)
			{
				apply = true;
				thd_log_debug("Passive Trip point applicable \n");
			}
			break;

		default:
			break;
	}

	// P state control are always affective
	if(type == P_T_STATE)
	{
		apply = true;
		thd_log_debug("P T states Trip point applicable \n");
	}

	if(apply)
	{
		if(read_temp >= temp)
		{
			thd_log_debug("Trip point applicable >  %d:%d \n", index, temp);
			on = 1;
		}
		else if((read_temp + hyst) < temp)
		{
			thd_log_debug("Trip point applicable =<  %d:%d \n", index, temp);
			off = 1;
		}
	}
	else
		return FALSE;

	if(on != 1 && off != 1)
		return TRUE;

	int i, ret;
	thd_log_debug("cdev size for this trippoint %d\n", cdevs.size());
	if(on > 0)
	{
		for(unsigned i = 0; i < cdevs.size(); ++i)
		{

			cthd_cdev *cdev = cdevs[i];
			thd_log_debug("cdev at index %d\n", cdev->thd_cdev_get_index());
			if(cdev->in_max_state())
			{
				thd_log_debug("Need to switch to next cdev \n");
				// No scope of control with this cdev
				continue;
			}
			ret = cdev->thd_cdev_set_state(temp, read_temp, 1, arg);
			if(control_type == SEQUENTIAL && ret == THD_SUCCESS)
			{
				// Only one cdev activation
				break;
			}
		}
	}

	if(off > 0)
	{
		for(i = cdevs.size() - 1; i >= 0; --i)
		{

			cthd_cdev *cdev = cdevs[i];
			thd_log_debug("cdev at index %d\n", cdev->thd_cdev_get_index());

			if(cdev->in_min_state())
			{
				thd_log_debug("Need to switch to next cdev \n");
				// No scope of control with this cdev
				continue;
			}
			cdev->thd_cdev_set_state(temp, read_temp, 0, arg);
			if(control_type == SEQUENTIAL)
			{
				// Only one cdev activation
				break;
			}
		}
	}

	return TRUE;
}

void cthd_trip_point::thd_trip_point_add_cdev(cthd_cdev &cdev)
{
	cdevs.push_back(&cdev);
}

int cthd_trip_point::thd_trip_point_add_cdev_index(int _index)
{
	cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(_index);
	if(cdev)
	{
		cdevs.push_back(cdev);
		return THD_SUCCESS;
	}
	else
	{
		thd_log_warn("thd_trip_point_add_cdev_index not present %d\n", _index);
		return THD_ERROR;
	}
}
