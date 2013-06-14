/*
 * thd_engine_dts.cpp: thermal engine class implementation
 *  for dts
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

#include "thd_zone_dts_sensor.h"
#include "thd_engine_dts.h"
#include "thd_cdev_tstates.h"
#include "thd_cdev_pstates.h"
#include "thd_cdev_msr_pstates.h"
#include "thd_cdev_turbo_states.h"
#include "thd_cdev_therm_sys_fs.h"
#include "thd_cdev_msr_turbo_states.h"
#include "thd_cdev_msr_pstates_limited.h"
#include "thd_cdev_intel_pstate_driver.h"
#include "thd_cdev_rapl.h"
#include "thd_cdev_msr_rapl.h"

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

/* This implements control using DTS sensor. It uses DTS sensor sysfs and
 * uses P and T states to control temperature. \
 * It looks for all the available cooling devices, if present then it assigns
 * an ID. These IDS are used in the zones to prioritize the order of cooling
 * methods.
 * Currently only supporting one physical package. As this daemon is
 * targeting portable clients/laptops, which tend to have one physical package.
 */

int cthd_engine_dts::read_thermal_zones()
{
	int count = zone_count;

#ifdef PER_CPU_P_STATE_CONTROL
	if (intel_pstate_driver_index == -1) {
		// If Intel p state driver is present, it removes per cpu
		// governor, so can't control P state for individual CPU

		// core cpu readings start from temp2.., temp1 is for package.
		// For per CPU compensation, we want to address individual cores
		for(int i = 2; i < cthd_zone_dts::max_dts_sensors; ++i)
		{
			std::stringstream temp_crit_str;
			temp_crit_str << "temp" << i << "_max";

			if(thd_sysfs.exists(temp_crit_str.str()))
			{
				cthd_zone_dts_sensor *zone = new cthd_zone_dts_sensor(count, i,
						"/sys/devices/platform/coretemp.0/");
				if(zone->zone_update() == THD_SUCCESS)
				{
					thd_log_info("DTS sensor %d present\n", i);
					zones.push_back(zone);
					++count;
				}
			}
		}
	}
	else
	{
		thd_log_info("Intel P state governor, will not allow per cpu P states\n ");
	}
#endif


	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/devices/platform/";

	if ((dir = opendir(base_path.c_str())) != NULL)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (!strncmp(entry->d_name, "coretemp.", strlen("coretemp.")))
			{
				cthd_zone_dts *zone = new cthd_zone_dts(count,
						base_path+entry->d_name+"/", atoi(entry->d_name+strlen("coretemp.")));
				zone->set_zone_active();
				if(zone->zone_update() == THD_SUCCESS)
				{
					zones.push_back(zone);
					++count;
				}
			}
		}
		closedir(dir);
	}

	if(!count)
	{
		// No coretemp sysfs exist, try hwmon
		thd_log_warn("Thermal DTS: No coretemp sysfs, trying hwmon \n");

		cthd_zone_dts *zone = new cthd_zone_dts(count,
				"/sys/class/hwmon/hwmon0/", 0);
		zone->set_zone_active();
		if(zone->zone_update() == THD_SUCCESS)
		{
			zones.push_back(zone);
			++count;
		}
		if (!count)
		{
			thd_log_error("Thermal DTS or hwmon: No Zones present: \n");
			return THD_FATAL_ERROR;
		}
	}

	return THD_SUCCESS;
}

int cthd_engine_dts::find_cdev_power_clamp()
{
	bool found = false;

	csys_fs cdev_sysfs("/sys/class/thermal/");

	for(int i = 0; i < max_cool_devs; ++i)
	{
		std::stringstream tcdev;
		tcdev << "cooling_device" << i << "/type";
		if(cdev_sysfs.exists(tcdev.str().c_str()))
		{
			std::string type_str;
			cdev_sysfs.read(tcdev.str(), type_str);
			if(type_str == "intel_powerclamp")
			{
				thd_log_debug("Found Intel power_camp: %s\n", type_str.c_str());

				cthd_sysfs_cdev *cdev = new cthd_sysfs_cdev(i, "/sys/class/thermal/");
				if(cdev->update() != THD_SUCCESS)
					return THD_ERROR;
				cdev->set_inc_dec_value(power_clamp_reduction_percent);
				cdev->set_down_adjust_control(true);
				cdevs.push_back(cdev);
				++cdev_cnt;
				power_clamp_index = i;
				found = true;
				break;
			}
		}
	}

	if (found)
		return THD_SUCCESS;
	else
		return THD_ERROR;
}

int cthd_engine_dts::find_cdev_rapl()
{
	bool found = false;

	csys_fs cdev_sysfs("/sys/class/thermal/");

	for(int i = 0; i < max_cool_devs; ++i)
	{
		std::stringstream tcdev;
		tcdev << "cooling_device" << i << "/type";
		if(cdev_sysfs.exists(tcdev.str().c_str()))
		{
			std::string type_str;
			std::string rapl_str = "rapl_pkg_";
			cdev_sysfs.read(tcdev.str(), type_str);

			unsigned found = type_str.find(rapl_str);
			if (found == (unsigned)std::string::npos) {
					continue;
			}
			std::string package_index_str = type_str.substr(rapl_str.length());
			int package_index = atoi(package_index_str.c_str());
			if (package_index >= max_package_support)
				continue;
			cthd_sysfs_cdev_rapl *cdev = new cthd_sysfs_cdev_rapl(i, "/sys/class/thermal/", package_index);
			if(cdev->update() != THD_SUCCESS)
				return THD_ERROR;
			cdevs.push_back(cdev);
			++cdev_cnt;
			intel_rapl_index[package_index] = i;
			thd_log_debug("Intel RAPL index = %d\n", intel_rapl_index[package_index]);

			cthd_sysfs_cdev_rapl_limited *cdev1 = new cthd_sysfs_cdev_rapl_limited(intel_rapl_limited_control_index * (package_index+1),
														i, "/sys/class/thermal/", package_index);
			cdev1->set_inc_dec_value(1000);
			if(cdev1->update() != THD_SUCCESS)
				return THD_ERROR;
			cdevs.push_back(cdev1);
			++cdev_cnt;
			intel_rapl_index_limited = intel_rapl_limited_control_index;
			thd_log_debug("Intel RAPL index limited = %d\n", intel_rapl_index_limited * (package_index+1));

			found = true;
		}
	}

	if (found)
		return THD_SUCCESS;
	else
		return THD_ERROR;
}

int cthd_engine_dts::check_intel_p_state_driver()
{
	csys_fs cdev_sysfs("/sys/devices/system/cpu/intel_pstate/");
	if (cdev_sysfs.exists())
	{
		thd_log_info("Found Intel pstate driver \n");
		cthd_intel_p_state_cdev *cdev = new cthd_intel_p_state_cdev(intel_pstate_control_index, "/sys/devices/system/cpu/intel_pstate/");
		if(cdev->update() != THD_SUCCESS)
			return THD_ERROR;
		cdevs.push_back(cdev);
		++cdev_cnt;
		intel_pstate_driver_index = 0;
	}

	return THD_SUCCESS;
}

int cthd_engine_dts::read_cooling_devices()
{

	find_cdev_rapl();
	find_cdev_power_clamp();
	check_intel_p_state_driver();

	if (processor_id_match()) {

		cthd_cdev_rapl_msr *cdev_rapl = new cthd_cdev_rapl_msr(msr_rapl_index,  -
					1);
		if(cdev_rapl->update() == THD_SUCCESS)
		{
			cdevs.push_back(cdev_rapl);
			++cdev_cnt;
		}

		// Turbo sub states control
		cthd_cdev_msr_turbo_states *cdev_turbo = new cthd_cdev_msr_turbo_states
				(msr_turbo_states_index,  - 1);
		if(cdev_turbo->update() == THD_SUCCESS)
		{
			cdevs.push_back(cdev_turbo);
			++cdev_cnt;
		}
		else
			msr_control_present = -1;
		// Instead of going to min limit to half
		cthd_cdev_pstate_msr_limited *cdevxx = new cthd_cdev_pstate_msr_limited(msr_p_states_index_limited,  -
				1);
		if(cdevxx->update() == THD_SUCCESS)
		{
			cdevs.push_back(cdevxx);
			++cdev_cnt;
		}
		else
			msr_control_present = -1;
		// P states control using MSRs
		cthd_cdev_pstate_msr *cdevx = new cthd_cdev_pstate_msr(msr_p_states_index,  -
					1);
		if(cdevx->update() == THD_SUCCESS)
		{
			cdevs.push_back(cdevx);
			++cdev_cnt;
		}
		else
			msr_control_present = -1;
		// Complete engage disengage turbo
		cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states(turbo_on_off_index,
				- 1);
		if(cdev0->update() == THD_SUCCESS)
		{
			cdevs.push_back(cdev0);
			++cdev_cnt;
		}
		else
			msr_control_present = -1;
	}
	else
	{
		thd_log_info("Not a sandybridge and above architecture \n");
		msr_control_present = -1;
	}
	// P states control using cpufreq
	cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cpufreq_index,  - 1);
	if(cdev1->update() == THD_SUCCESS)
	{
		cdevs.push_back(cdev1);
		++cdev_cnt;
	}

	// T states control
	cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(t_state_index,  - 1);
	if(cdev2->update() == THD_SUCCESS)
	{
		cdevs.push_back(cdev2);
		++cdev_cnt;
	}
	else
		msr_control_present = -1;

	if (msr_control_present < 0)
	{
		thd_log_warn("MSR CONTROL WILL BE DISABLED \n");
	}

	if(!cdev_cnt)
	{
		thd_log_error("Thermal DTS: No cooling devices: \n");
		return THD_FATAL_ERROR;
	}

	// Individual CPU controls
	csys_fs cpu_sysfs("/dev/cpu/");

	for(int i = 0; i < max_cpu_count; ++i)
	{
		std::stringstream file_name_str;

		file_name_str << i;
		if(cpu_sysfs.exists(file_name_str.str()))
		{
			last_cpu_update[i] = 0;
			if (processor_id_match()) {
				// Turbo sub states control
				cthd_cdev_msr_turbo_states *cdev_turbo = new cthd_cdev_msr_turbo_states
						(msr_turbo_states_index + max_cpu_count *(i + 1), i);
				if(cdev_turbo->update() == THD_SUCCESS)
				{
					cdevs.push_back(cdev_turbo);
					++cdev_cnt;
				}
				else
					msr_control_present = -1;

				cthd_cdev_pstate_msr_limited *cdevxx = new cthd_cdev_pstate_msr_limited(msr_p_states_index_limited +
						max_cpu_count *(i + 1), i);
				if(cdevxx->update() == THD_SUCCESS)
				{
					cdevs.push_back(cdevxx);
					++cdev_cnt;
				}
				else
					msr_control_present = -1;

				//p states
				cthd_cdev_pstate_msr *cdevx = new cthd_cdev_pstate_msr(msr_p_states_index +
							max_cpu_count *(i + 1), i);
				if(cdevx->update() == THD_SUCCESS)
				{
					cdevs.push_back(cdevx);
					++cdev_cnt;
				}
				else
					msr_control_present = -1;

				// Complete engage disengage turbo
				cthd_cdev_turbo_states *cdev0 = new cthd_cdev_turbo_states
							(turbo_on_off_index + max_cpu_count *(i + 1), i);
				if(cdev0->update() == THD_SUCCESS)
				{
					cdevs.push_back(cdev0);
					++cdev_cnt;
				}
				else
					msr_control_present = -1;
			}
			else
				msr_control_present = -1;

			// P states control using cpufreq
			cthd_cdev_pstates *cdev1 = new cthd_cdev_pstates(cpufreq_index +
													max_cpu_count *(i + 1), i);
			if(cdev1->update() == THD_SUCCESS)
			{
				cdevs.push_back(cdev1);
				++cdev_cnt;
			}
			// T states control
			cthd_cdev_tstates *cdev2 = new cthd_cdev_tstates(t_state_index +
										max_cpu_count *(i + 1), i);
			if(cdev2->update() == THD_SUCCESS)
			{
				cdevs.push_back(cdev2);
				++cdev_cnt;
			}
			else
				msr_control_present = -1;
		}
	}

	return THD_SUCCESS;
}

bool cthd_engine_dts::apply_cpu_operation(int cpu)
{
	time_t tm;

	time(&tm);
	if(last_cpu_update[cpu] == 0)
	{
		last_cpu_update[cpu] = tm;
		return true;
	}
	if(abs(tm - last_cpu_update[cpu]) > 2)
	{
		last_cpu_update[cpu] = tm;
		return true;
	}
	else
	{
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
