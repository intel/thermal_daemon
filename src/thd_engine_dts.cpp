/*
 * thd_engine_dts.cpp: thermal engine class implementation
 *  for dts
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

#include "thd_zone_dts.h"
#include "thd_engine_dts.h"
#include "thd_cdev_tstates.h"
#include "thd_cdev_pstates.h"
#include "thd_cdev_turbo_states.h"

int cthd_engine_dts::read_thermal_zones()
{
	int count = zone_count;

	cthd_zone_dts *zone = new cthd_zone_dts(count, "/sys/devices/platform/coretemp.0/");
	if (zone->zone_update() == THD_SUCCESS) {
		zones.push_back(zone);
			++count;
	}

	if (!count) {
		thd_log_error("Thermal DTS: No Zones present: \n");
		return THD_FATAL_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_engine_dts::read_cooling_devices()
{
#ifdef TURBO_HIGH_PRIORITY
	cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states(cdev_cnt);
	if (cdev0->update() == THD_SUCCESS) {
			cdevs.push_back(cdev0);
			++cdev_cnt;
	}

	cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cdev_cnt);
	if (cdev1->update() == THD_SUCCESS) {
			cdevs.push_back(cdev1);
			++cdev_cnt;
	}
#else
	cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cdev_cnt);
	if (cdev1->update() == THD_SUCCESS) {
			cdevs.push_back(cdev1);
			++cdev_cnt;
	}
	cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states(cdev_cnt);
	if (cdev0->update() == THD_SUCCESS) {
			cdevs.push_back(cdev0);
			++cdev_cnt;
	}

#endif
	cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(cdev_cnt);
	if (cdev2->update() == THD_SUCCESS) {
			cdevs.push_back(cdev2);
			++cdev_cnt;
	}

	if (!cdev_cnt) {
		thd_log_error("Thermal DTS: No cooling devices: \n");
		return THD_FATAL_ERROR;
	}

	return THD_SUCCESS;
}
