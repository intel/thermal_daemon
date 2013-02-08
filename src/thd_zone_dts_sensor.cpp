/*
 * thd_zone_dts_sensor.cpp: thermal engine DTS sensor class implementation
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

#include "thd_zone_dts_sensor.h"
#include "thd_engine_dts.h"

cthd_zone_dts_sensor::cthd_zone_dts_sensor(int count, int _index, std::string
	path): cthd_zone_dts(count, path), index(_index), conf_present(false){

}

unsigned int cthd_zone_dts_sensor::read_cpu_mask()
{
	std::stringstream filename;
	unsigned int mask = 0;

	filename << TDCONFDIR << "/" << "dts_" << index << "_sensor_mask.conf";
	thd_log_debug("read_cpu_mask file name %s\n", filename.str().c_str());
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if(ifs.good())
	{
		ifs >> mask;
		conf_present = true;
	}
	ifs.close();
	thd_log_info("DTS %d mask %x\n", index, mask);

	return mask;
}

int cthd_zone_dts_sensor::read_trip_points()
{
	unsigned int cpu_mask;
	unsigned int mask = 0x1;
	int cdev_index;
	int cnt = 0;
	cthd_engine_dts *thd_dts_engine = (cthd_engine_dts*)thd_engine;

	init();

	cpu_mask = read_cpu_mask();
	do
	{
		if(cpu_mask &mask)
		{
			cthd_trip_point trip_pt(trip_point_cnt, P_T_STATE, set_point, def_hystersis,
	index);
			trip_pt.thd_trip_point_set_control_type(SEQUENTIAL);

			trip_pt.thd_trip_point_add_cdev_index(thd_dts_engine->msr_turbo_states_index
	+ thd_dts_engine->max_cpu_count *(cnt + 1));
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
			cnt++;
		}
		mask = (mask << 1);
	}
	while(mask != 0);

	if(cnt)
	{
		thd_log_debug("Setting Zone to Active: sensor index %d\n", index);
		set_zone_active();
	}
	if(conf_present)
	{
		cthd_engine_dts *engine = (cthd_engine_dts*)thd_engine;

		thd_log_debug("engine->sensor_mask before %x index %x \n", engine
	->sensor_mask, index);

		engine->sensor_mask |= (1 << index);
		thd_log_debug("engine->sensor_mask %x\n", engine->sensor_mask);
		((cthd_engine_dts*)thd_engine)->remove_cpu_mask_from_default_processing
	(cpu_mask);
	}
	return THD_SUCCESS;
}

unsigned int cthd_zone_dts_sensor::read_zone_temp()
{
	unsigned int mask = 0x1;
	int cnt = 0;

	zone_temp = 0;
	std::stringstream temp_input_str;
	temp_input_str << "temp" << index << "_input";
	if(dts_sysfs.exists(temp_input_str.str()))
	{
		std::string temp_str;
		dts_sysfs.read(temp_input_str.str(), temp_str);
		std::istringstream(temp_str) >> zone_temp;
		thd_log_debug("dts_sensor %d: zone_temp %u\n", index, zone_temp);
	}

	thd_model.add_sample(zone_temp);
	if(thd_model.is_set_point_reached())
	{
		prev_set_point = set_point;
		set_point = thd_model.get_set_point();
		thd_log_debug("new set point %d \n", set_point);
		update_trip_points();
	}

	return zone_temp;
}
