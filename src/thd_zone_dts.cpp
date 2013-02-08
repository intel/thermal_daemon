/*
 * thd_zone_dts.cpp: thermal engine DTS class implementation
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
 * This implementation allows to use core temperature interface.
 */

#include "thd_zone_dts.h"
#include "thd_engine_dts.h"
#include "thd_msr.h"

cthd_zone_dts::cthd_zone_dts(int index, std::string path): cthd_zone(index,
	path), dts_sysfs(path.c_str()), trip_point_cnt(0){

}

int cthd_zone_dts::init()
{
	critical_temp = 0;
	int temp;
	cthd_engine_dts *engine = (cthd_engine_dts*)thd_engine;

	for(int i = 0; i < max_dts_sensors; ++i)
	{
		std::stringstream temp_crit_str;
		//		temp_crit_str << "temp" << i << "_crit";
		temp_crit_str << "temp" << i << "_max";

		if(dts_sysfs.exists(temp_crit_str.str()))
		{

			// Set which index is present
			sensor_mask = sensor_mask | (1 << i);

			std::string temp_str;
			dts_sysfs.read(temp_crit_str.str(), temp_str);
			std::istringstream(temp_str) >> temp;
			if(critical_temp == 0 || temp < critical_temp)
				critical_temp = temp;
		}
	}
	thd_log_debug("Core temp DTS :critical %d\n", critical_temp);

	thd_model.set_max_temperature(critical_temp);
	prev_set_point = set_point = thd_model.get_set_point();

	update_zone_preference();

	return THD_SUCCESS;
}

int cthd_zone_dts::read_trip_points()
{
	cthd_msr msr;
	int _index = 0;

	init();
	cthd_engine_dts *thd_dts_engine = (cthd_engine_dts*)thd_engine;

	unsigned int cpu_mask = thd_dts_engine->get_cpu_mask();
	thd_log_info("Default DTS processing for cpus mask = %x\n", cpu_mask);

	if(cpu_mask == thd_dts_engine->def_cpu_mask)
	{
		cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
	0xFF);
		trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
		trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_turbo_states_index);
		trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index);
		trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->turbo_on_off_index);
#ifdef CPU_FREQ
		trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->cpufreq_index);
#endif 
		if(thd_dts_engine->power_clamp_index !=  - 1)
		{
			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->power_clamp_index);
		}
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
				cdev_index = cpu_to_cdev_index(cnt);
				cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
	0xFF);
				trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine
	->msr_turbo_states_index + thd_dts_engine->max_cpu_count *(cnt + 1));
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_p_states_index +
	thd_dts_engine->max_cpu_count *(cnt + 1));
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->turbo_on_off_index +
	thd_dts_engine->max_cpu_count *(cnt + 1));
#ifdef CPU_FREQ
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->cpufreq_index +
	thd_dts_engine->max_cpu_count *(cnt + 1));
#endif 
				if(thd_dts_engine->power_clamp_index !=  - 1)
				{
					trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->power_clamp_index);
				}
				trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->t_state_index +
	thd_dts_engine->max_cpu_count *(cnt + 1));
				trip_points.push_back(trip_pt);
				trip_point_cnt++;
			}
			mask = (mask << 1);
			cnt++;
			if(cnt >= msr.get_no_cpus())
				break;
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
	unsigned int mask = 0x1;
	unsigned int temp;
	int cnt = 0;
	cthd_engine_dts *engine = (cthd_engine_dts*)thd_engine;

	zone_temp = 0;
	do
	{
		if((sensor_mask &~engine->sensor_mask) &mask)
		{
			std::stringstream temp__input_str;
			temp__input_str << "temp" << cnt << "_input";
			if(dts_sysfs.exists(temp__input_str.str()))
			{
				std::string temp_str;
				dts_sysfs.read(temp__input_str.str(), temp_str);
				std::istringstream(temp_str) >> temp;
				if(zone_temp == 0 || temp > zone_temp)
					zone_temp = temp;
				thd_log_debug("dts %d: temp %u zone_temp %u\n", cnt, temp, zone_temp);
			}
		}
		mask = (mask << 1);
		cnt++;
	}
	while(mask != 0);

	thd_model.add_sample(zone_temp);
	if(thd_model.is_set_point_reached())
	{
		prev_set_point = set_point;
		set_point = thd_model.get_set_point();
		thd_log_debug("new set point %d \n", set_point);
		update_trip_points();
	}
#if 1
	{
		//	cthd_msr msr;
		//	thd_log_debug("perf_status %X\n", msr.read_perf_status());

		{

			//	topology.learn_topology();

		}
	}
#endif 

	return zone_temp;
}

void cthd_zone_dts::update_zone_preference()
{
	cthd_preference thd_pref;
	int pref;
	cthd_msr msr;

	pref = thd_pref.get_preference();
	switch(pref)
	{
		case PREF_BALANCED:
			msr.set_perf_bias_balaced();
			break;
		case PREF_PERFORMANCE:
			msr.set_perf_bias_performace();
			break;
		case PREF_ENERGY_CONSERVE:
			msr.set_perf_bias_energy();
			break;
		default:
			break;
	}
	cthd_zone::update_zone_preference();
}
