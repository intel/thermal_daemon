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

#include "thd_zone_dts_sensor.h"
#include "thd_engine_dts.h"
#include "thd_cdev_tstates.h"
#include "thd_cdev_pstates.h"
#include "thd_cdev_msr_pstates.h"
#include "thd_cdev_turbo_states.h"

#define TURBO_HIGH_PRIORITY

int cthd_engine_dts::read_thermal_zones()
{
	int count = zone_count;

	for (int i=0; i<cthd_zone_dts::max_dts_sensors; ++i) {
		std::stringstream temp_crit_str;
		temp_crit_str << "temp" << i << "_max";

		if (thd_sysfs.exists(temp_crit_str.str())) {
			cthd_zone_dts_sensor *zone = new cthd_zone_dts_sensor(count, i, "/sys/devices/platform/coretemp.0/");
			if (zone->zone_update() == THD_SUCCESS) {
				thd_log_info("DTS sensor %d present\n", i);
				zones.push_back(zone);
					++count;
			}
		}
	}

	cthd_zone_dts *zone = new cthd_zone_dts(count, "/sys/devices/platform/coretemp.0/");
	zone->set_zone_active();
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
	// Turbo sub states control including p states using MSRs
	cthd_cdev_pstate_msr *cdevx = new cthd_cdev_pstate_msr(cdev_cnt, -1);
	if (cdevx->update() == THD_SUCCESS) {
			cdevs.push_back(cdevx);
			++cdev_cnt;
	}

	// Complete engage disengage turbo
	cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states(cdev_cnt, -1);
	if (cdev0->update() == THD_SUCCESS) {
			cdevs.push_back(cdev0);
			++cdev_cnt;
	}

	// P states control using cpufreq
	cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cdev_cnt, -1);
	if (cdev1->update() == THD_SUCCESS) {
			cdevs.push_back(cdev1);
			++cdev_cnt;
	}

	// T states control
	cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(cdev_cnt, -1);
	if (cdev2->update() == THD_SUCCESS) {
			cdevs.push_back(cdev2);
			++cdev_cnt;
	}

	if (!cdev_cnt) {
		thd_log_error("Thermal DTS: No cooling devices: \n");
		return THD_FATAL_ERROR;
	}

	// Individual CPU controls
	csys_fs cpu_sysfs("/dev/cpu/");

	for(int i=0; i<max_cpu_count; ++i) {
		std::stringstream file_name_str;

		file_name_str << i;
		if (cpu_sysfs.exists(file_name_str.str())) {
			last_cpu_update[i] = 0;

			// Turbo sub states control including p states using MSRs
			cthd_cdev_pstate_msr *cdevx = new cthd_cdev_pstate_msr(cdev_cnt, i);
			if (cdevx->update() == THD_SUCCESS) {
					cdevs.push_back(cdevx);
					++cdev_cnt;
			}

			// Complete engage disengage turbo
			cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states(cdev_cnt, i);
			if (cdev0->update() == THD_SUCCESS) {
					cdevs.push_back(cdev0);
					++cdev_cnt;
			}

			// P states control using cpufreq
			cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cdev_cnt, i);
			if (cdev1->update() == THD_SUCCESS) {
					cdevs.push_back(cdev1);
					++cdev_cnt;
			}

			// T states control
			cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(cdev_cnt, i);
			if (cdev2->update() == THD_SUCCESS) {
					cdevs.push_back(cdev2);
					++cdev_cnt;
			}
		}
	}


#else
	cthd_cdev_pstate_msr *cdevx = new cthd_cdev_pstate_msr(cdev_cnt);
	if (cdevx->update() == THD_SUCCESS) {
			cdevs.push_back(cdevx);
			++cdev_cnt;
	}

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

	// T states control
	cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(cdev_cnt);
	if (cdev2->update() == THD_SUCCESS) {
			cdevs.push_back(cdev2);
			++cdev_cnt;
	}

	if (!cdev_cnt) {
		thd_log_error("Thermal DTS: No cooling devices: \n");
		return THD_FATAL_ERROR;
	}
#endif
	return THD_SUCCESS;
}

bool cthd_engine_dts::apply_cpu_operation(int cpu)
{
	time_t tm;

	time(&tm);
	if (last_cpu_update[cpu] == 0) {
		last_cpu_update[cpu] = tm;
		return true;
	}
	if (abs(tm - last_cpu_update[cpu]) > 2) {
		last_cpu_update[cpu] = tm;
		return true;
	}
	else {
		thd_log_info("Too early op for cpu %d \n", cpu);
		return false;
	}
}

void cthd_engine_dts::remove_cpu_mask_from_default_processing(unsigned int mask)
{
	cpu_mask_remaining &= ~mask;
}

unsigned int cthd_engine_dts::get_cpu_mask()
{
	return cpu_mask_remaining;
}
