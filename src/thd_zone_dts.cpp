/*
 * thd_zone_dts.cpp: thermal engine DTS class implementation
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
 * This implementation allows to use core temperature interface.
 */

/* Implementation of DTS sensor Zone. This
 * - Identifies DTS sensors, and use maximum reported temperature to control
 * - Prioritize the cooling device order
 * - Sets one control point starting from target temperature (max) not critical.
 * Very first time it reaches this temperature cthd_model, calculates a set point when
 * cooling device needs to be activated.
 *
 */

#include <dirent.h>
#include "thd_zone_dts.h"
#include "thd_engine_dts.h"
#include "thd_msr.h"
#include "thd_cdev_order_parser.h"

cthd_zone_dts::cthd_zone_dts(int index, std::string path, int package_id): cthd_zone(index,
	path), dts_sysfs(path.c_str()), trip_point_cnt(0), sensor_mask(0), phy_package_id(package_id),
	thd_model(true), pkg_thres_th_zone(-1) {
	thd_log_debug("zone dts syfs: %s, package id %d \n", path.c_str(), package_id);
}

int cthd_zone_dts::init()
{
	critical_temp = 0;
	int temp;
	cthd_engine_dts *engine = (cthd_engine_dts*)thd_engine;
	bool found = false;

	max_temp = 0;
	critical_temp = 0;

	for(int i = 0; i < max_dts_sensors; ++i)
	{
		std::stringstream temp_crit_str;
		std::stringstream temp_max_str;

		temp_crit_str << "temp" << i << "_crit";
		temp_max_str << "temp" << i << "_max";

		if(dts_sysfs.exists(temp_crit_str.str()))
		{
			std::string temp_str;
			dts_sysfs.read(temp_crit_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if(critical_temp == 0 || temp < critical_temp)
				critical_temp = temp;

		}

		if(dts_sysfs.exists(temp_max_str.str()))
		{

			// Set which index is present
			sensor_mask = sensor_mask | (1 << i);

			std::string temp_str;
			dts_sysfs.read(temp_max_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if(max_temp == 0 || temp < max_temp)
				max_temp = temp;
			found = true;
		} else if(dts_sysfs.exists(temp_crit_str.str()))
		{
			thd_log_info("DTS: MAX target temp not found, using (crit - offset) \n");
			// Set which index is present
			sensor_mask = sensor_mask | (1 << i);

			std::string temp_str;
			dts_sysfs.read(temp_crit_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			// Adjust offset from critical (target temperature for cooling)
			temp = temp - def_offset_from_critical;
			if(max_temp == 0 || temp < max_temp)
				max_temp = temp;
			found = true;
		}
	}
	if(!found)
	{
		thd_log_error("DTS temperature path not found \n");
		return THD_ERROR;
	}
	thd_log_debug("Core temp DTS :critical %d, max %d\n", critical_temp, max_temp);

	thd_model.set_max_temperature(max_temp);
	prev_set_point = set_point = thd_model.get_set_point();

	pkg_thres_th_zone = check_for_package_temp_thermal_zone();
	if(pkg_thres_th_zone > 0)
	{
		thd_log_info("DTS package temp thermal zone found\n");
		set_package_temp_thermal_zone_thres(pkg_thres_th_zone,
				thd_model.get_hot_zone_trigger_point(), 0);
		set_package_temp_zone_policy(pkg_thres_th_zone, "user_space");
	} else
		thd_log_info("DTS package temp thermal zone not found, Requires polling !!\n");


	return THD_SUCCESS;
}

int cthd_zone_dts::load_cdev_xml(cthd_trip_point &trip_pt, std::vector <std::string> &list)
{
	cthd_engine_dts *thd_dts_engine = (cthd_engine_dts*)thd_engine;

	for (int i = 0; i < list.size(); ++i) {
		thd_log_debug("- %s\n", list[i].c_str());
		if (list[i] == "CDEV_INTEL_RAPL_DRIVER")
		{
			if (thd_dts_engine->intel_rapl_index[phy_package_id] != -1)
			{
				thd_log_debug("ZONE DTS add RAPL id %d \n", thd_dts_engine->intel_rapl_index[phy_package_id]);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->intel_rapl_index[phy_package_id]);
			}
		}
		else if (list[i] == "CDEV_RAPL")
		{
			if (thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_RAPL %d\n", thd_dts_engine->msr_rapl_index);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_rapl_index);
			}
		}
		else if (list[i] == "CDEV_INTEL_PSTATE_DRIVER")
		{
			if (thd_dts_engine->intel_pstate_driver_index != -1)
			{
				thd_log_debug("add trip CDEV_INTEL_PSTATE_DRIVER %d\n", thd_dts_engine->intel_pstate_control_index);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->intel_pstate_control_index);
			}
		}
		else if (list[i] == "CDEV_MSR_TURBO_STATES")
		{
			if (thd_dts_engine->intel_pstate_driver_index == -1 && thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_MSR_TURBO_STATES %d\n", thd_dts_engine->msr_turbo_states_index);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_turbo_states_index);
			}
		}
		else if (list[i] == "CDEV_MSR_PSTATES_PARTIAL")
		{
			if (thd_dts_engine->intel_pstate_driver_index == -1 && thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_MSR_PSTATES_PARTIAL %d\n", thd_dts_engine->msr_p_states_index_limited);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index_limited);
			}
		}
		else if (list[i] == "CDEV_TURBO_ON_OFF")
		{
			if (thd_dts_engine->intel_pstate_driver_index == -1 && thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_TURBO_ON_OFF %d\n", thd_dts_engine->turbo_on_off_index);
 				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->turbo_on_off_index);
			}
		}
		else if (list[i] == "CDEV_CPU_FREQ")
		{
			thd_log_debug("add trip CDEV_CPU_FREQ %d\n", thd_dts_engine->cpufreq_index);
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->cpufreq_index);
		}
		else if (list[i] == "CDEV_POWER_CLAMP")
		{
			if(thd_dts_engine->power_clamp_index !=  - 1)
			{
				thd_log_debug("add trip CDEV_POWER_CLAMP %d\n", thd_dts_engine->power_clamp_index);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->power_clamp_index);
			}
		}
		else if (list[i] == "CDEV_MSR_PSTATES")
		{
			if (thd_dts_engine->intel_pstate_driver_index == -1 && thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_PSTATES %d\n", thd_dts_engine->msr_p_states_index_limited);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index_limited);
			}
		}
		else if (list[i] == "CDEV_T_STATES")
		{
			if (thd_dts_engine->msr_control_present > 0)
			{
				thd_log_debug("add trip CDEV_T_STATES %d\n", thd_dts_engine->t_state_index);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->t_state_index);
			}
		}
	}			

}

int cthd_zone_dts::parse_cdev_order()
{
	cthd_cdev_order_parse parser;
	std::vector <std::string> order_list;
	int ret = THD_ERROR;


	if((ret = parser.parser_init()) == THD_SUCCESS)
	{
		if((ret = parser.start_parse()) == THD_SUCCESS)
		{
			ret = parser.get_order_list(order_list);
			if (ret == THD_SUCCESS)
			{
				cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
							0xFF);
				trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
				load_cdev_xml(trip_pt, order_list);
				trip_points.push_back(trip_pt);
				trip_point_cnt++;
			}
		}
		parser.parser_deinit();
		return ret;
	}

	return ret;	
}

int cthd_zone_dts::read_trip_points()
{
	cthd_msr msr;
	int _index = 0;
	int ret;

	if (init() != THD_SUCCESS)
		return THD_ERROR;
	cthd_engine_dts *thd_dts_engine = (cthd_engine_dts*)thd_engine;

	unsigned int cpu_mask = thd_dts_engine->get_cpu_mask();
	thd_log_info("Default DTS processing for cpus mask = %x\n", cpu_mask);

	if(cpu_mask == thd_dts_engine->def_cpu_mask)
	{

		ret = parse_cdev_order();
		if (ret == THD_SUCCESS) {
			thd_log_info("CDEVS order specified in thermal-cdev-order.xml\n");
			goto cdev_loaded;
		}
		cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
	0xFF);
		trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);

		if (thd_dts_engine->intel_rapl_index[phy_package_id] != -1)
		{
			thd_log_debug("ZONE DTS add RAPL id %d \n", thd_dts_engine->intel_rapl_index[phy_package_id]);
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->intel_rapl_index[phy_package_id]);
		}
		else if (thd_dts_engine->msr_control_present > 0)
		{
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_rapl_index);
		}

		if (thd_dts_engine->intel_pstate_driver_index != -1)
		{
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->intel_pstate_control_index);
		}
		else if (thd_dts_engine->msr_control_present > 0)
		{
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_turbo_states_index);
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index_limited);
		}
		if (thd_dts_engine->msr_control_present > 0)
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->turbo_on_off_index);
		else
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->cpufreq_index);
		if(thd_dts_engine->power_clamp_index !=  - 1)
		{
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->power_clamp_index);
		}
		if (thd_dts_engine->intel_pstate_driver_index == -1)
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index);
		if (thd_dts_engine->msr_control_present > 0)
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->t_state_index);
		trip_points.push_back(trip_pt);
		trip_point_cnt++;
	}
	else
	{
		unsigned int mask = 0x1;
		int cnt = 0;
		int cdev_index;
		do
		{
			if(cpu_mask &mask)
			{
				thd_log_debug("Add default processing for %x\n", cpu_mask & mask);
				cdev_index = cpu_to_cdev_index(cnt);
				cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
	0xFF);
				trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
				if (thd_dts_engine->msr_control_present > 0)
				{
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine
							->msr_turbo_states_index + thd_dts_engine->max_cpu_count *(cnt + 1));
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index_limited +
							thd_dts_engine->max_cpu_count *(cnt + 1));
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->turbo_on_off_index +
							thd_dts_engine->max_cpu_count *(cnt + 1));
				}
				else
				{
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->cpufreq_index +
						thd_dts_engine->max_cpu_count *(cnt + 1));
				}
				if(thd_dts_engine->power_clamp_index !=  - 1)
				{
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->power_clamp_index);
				}
				if (thd_dts_engine->msr_control_present > 0)
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->t_state_index +
						thd_dts_engine->max_cpu_count *(cnt + 1));
				trip_points.push_back(trip_pt);
				trip_point_cnt++;
			}
			mask = (mask << 1);
			cnt++;
			if(cnt >= msr.get_no_cpus()) {
				thd_log_debug("default mask processing done at cnt=%d\n", cnt);
				break;
			}
		}
		while(mask != 0);
	}
cdev_loaded:
	{
		int cnt = 0;
		unsigned int mask = 0x1;
		do
		{
			if((sensor_mask & ~thd_dts_engine->sensor_mask) &mask)
			{
				std::stringstream temp_input_str;
				temp_input_str << "temp" << cnt << "_input";
				if(dts_sysfs.exists(temp_input_str.str()))
				{
					sensor_sysfs.push_back(temp_input_str.str());
				}
			}
			mask = (mask << 1);
			cnt++;
		}
		while(mask != 0);
	}
	return THD_SUCCESS;
}

int cthd_zone_dts::update_trip_points()
{
	if(prev_set_point == set_point)
		return THD_SUCCESS;

	for(int i = 0; i < trip_point_cnt; ++i)
	{
		cthd_trip_point &trip_pt = trip_points[i];
		if(trip_pt.thd_trip_point_value() == prev_set_point)
		{
			trip_pt.thd_trip_update_set_point(set_point);
			break;
		}
	}
	if(pkg_thres_th_zone > 0)
	{
		thd_log_info("Updating temp notify thresholds \n");
		set_package_temp_thermal_zone_thres(pkg_thres_th_zone,
				thd_model.get_hot_zone_trigger_point(), 0);
	}
	return THD_SUCCESS;
}

int cthd_zone_dts::read_cdev_trip_points()
{
	return THD_SUCCESS;

}

void cthd_zone_dts::set_temp_sensor_path(){

}

unsigned int cthd_zone_dts::read_zone_temp()
{
	unsigned int temp;
	int cnt = sensor_sysfs.size();

	zone_temp = 0;

	for(int i=0; i<cnt; ++i)
	{
		dts_sysfs.read(sensor_sysfs[i], &temp);
		if(zone_temp == 0 || temp > zone_temp)
			zone_temp = temp;
		thd_log_debug("dts %d: temp %u zone_temp %u\n", i, temp, zone_temp);

	}

	thd_model.add_sample(zone_temp);
	if(thd_model.is_set_point_reached())
	{
		prev_set_point = set_point;
		set_point = thd_model.get_set_point();
		thd_log_debug("new set point %d \n", set_point);
		update_trip_points();
	}
	if(pkg_thres_th_zone > 0)
	{
		if(zone_temp > thd_model.get_hot_zone_trigger_point())
			thd_engine->thd_engine_poll_enable();
		else
			thd_engine->thd_engine_poll_disable();
	}
	return zone_temp;
}

void cthd_zone_dts::update_zone_preference()
{
	thd_model.update_user_set_max_temp();
	cthd_zone::update_zone_preference();
}

int cthd_zone_dts::check_for_package_temp_thermal_zone()
{
	int index = -1;
	csys_fs sysfs("/sys/class/thermal/");
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int cnt = 0;
	if ((dir = opendir(base_path.c_str())) != NULL)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (!strncmp(entry->d_name, "thermal_zone", strlen("thermal_zone")))
			{
				int i;

				i = atoi(entry->d_name+strlen("thermal_zone"));
				std::stringstream type;
				std::string type_val;

				type << "thermal_zone" << i << "/type";
				if(sysfs.exists(type.str().c_str()))
				{
					sysfs.read(type.str(), type_val);
				}
				if (!type_val.compare(0, 9, "pkg-temp-")) {
					index = i;
					break;
				}
			}
		}
	}

	return index;
}

int cthd_zone_dts::set_package_temp_thermal_zone_thres(int zone, int thres_0, int thres_1)
{
	std::stringstream tcdev0;
	std::stringstream tcdev1;
	csys_fs cdev_sysfs("/sys/class/thermal/");
	std::stringstream thres0;
	std::stringstream thres1;
	int error = THD_ERROR;

	thres0 << thres_0;
	tcdev0 << "thermal_zone" << zone << "/trip_point_0_temp";
	if(cdev_sysfs.exists(tcdev0.str().c_str()))
	{
		cdev_sysfs.write(tcdev0.str(), thres0.str());
		error = THD_SUCCESS;
	}
	thres1 << thres_1;
	tcdev1 << "thermal_zone" << zone << "/trip_point_1_temp";
	if(cdev_sysfs.exists(tcdev1.str().c_str()))
	{
		cdev_sysfs.write(tcdev1.str(), thres1.str());
		error = THD_SUCCESS;
	}

	return error;
}

int cthd_zone_dts::set_package_temp_zone_policy(int zone, std::string policy)
{
	csys_fs cdev_sysfs("/sys/class/thermal/");
	int error = THD_ERROR;
	std::stringstream policy_sysfs;

	policy_sysfs << "thermal_zone" << zone << "/policy";
	if(cdev_sysfs.exists(policy_sysfs.str().c_str()))
	{
		cdev_sysfs.write(policy_sysfs.str(), policy);
		error = THD_SUCCESS;
	}

	return error;
}
