/*
 * thd_sensor_virtual.h: thermal sensor virtual class implementation
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#include "thd_sensor_virtual.h"
#include "thd_engine.h"

cthd_sensor_virtual::cthd_sensor_virtual(int _index, std::string _type_str,
		std::string _link_type_str, double _multiplier, double _offset) :
		cthd_sensor(_index, "none", std::move(_type_str)), link_sensor(NULL), link_type_str(
				std::move(_link_type_str)), multiplier(_multiplier), offset(_offset) {
	virtual_sensor = true;
}

cthd_sensor_virtual::~cthd_sensor_virtual() {
}

int cthd_sensor_virtual::sensor_update() {
	cthd_sensor *sensor = thd_engine->search_sensor(link_type_str);

	if (sensor)
		link_sensor = sensor;
	else
		return THD_ERROR;

	return THD_SUCCESS;
}

unsigned int cthd_sensor_virtual::read_temperature() {
	unsigned int temp;

	temp = link_sensor->read_temperature();

	temp = temp * multiplier + offset;

	thd_log_debug("cthd_sensor_virtual::read_temperature %u\n", temp);

	return temp;
}

int cthd_sensor_virtual::sensor_update_param(std::string new_dep_sensor, double slope, double intercept)
{
	cthd_sensor *sensor = thd_engine->search_sensor(std::move(new_dep_sensor));

	if (sensor)
		link_sensor = sensor;
	else
		return THD_ERROR;

	multiplier = slope;
	offset = intercept;

	return THD_SUCCESS;
}
