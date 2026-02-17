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
#include <cmath>

cthd_sensor_virtual::cthd_sensor_virtual(int _index, std::string _type_str,
		std::string& _link_type_str, double _multiplier, double _offset) :
		cthd_sensor(_index, "none", std::move(_type_str))
				, multiplier(_multiplier), offset(_offset), poll_thread(0) {

	last_temp = 0;
	polling = 0;
	polling_period = def_polling_period;

	if (!_link_type_str.empty()) {
		link_sensor_t *link_sensor = new (link_sensor_t);
		if (!link_sensor)
			return;

		cthd_sensor *sensor = thd_engine->search_sensor(_link_type_str);
		link_sensor->sensor = sensor;
		link_sensor->offset = 0;
		link_sensor->coeff = 0;
		link_sensor->prev_avg = 0.0;
		link_sensors.push_back(link_sensor);
	}

	virtual_sensor = true;
}

cthd_sensor_virtual::~cthd_sensor_virtual() {
	for (unsigned int i = 0; i < link_sensors.size(); ++i) {
		link_sensor_t *link_sensor = link_sensors[i];
		delete link_sensor;
	}
	link_sensors.clear();
}

int cthd_sensor_virtual::add_target(std::string& _link_type_str, double coeff, double offset, int power_sensor)
{
		cthd_sensor *sensor = thd_engine->search_sensor(_link_type_str);
		if (sensor) {
			link_sensor_t *link_sensor = new (link_sensor_t);
			if (!link_sensor)
				return THD_ERROR;

			link_sensor->sensor = sensor;
			link_sensor->offset = offset;
			link_sensor->coeff = coeff;
			link_sensor->prev_avg = 0.0;
			link_sensor->power_sensor = power_sensor;
			link_sensors.push_back(link_sensor);
		}

		return THD_ERROR;
}

int cthd_sensor_virtual::sensor_update() {
	thd_log_debug("Add virtual sensor %s\n", type_str.c_str());

	for (unsigned int i = 0; i < link_sensors.size(); ++i) {
		link_sensor_t *link_sensor = link_sensors[i];

		if (link_sensor->sensor) {
			thd_log_debug("Add target sensor %s, %g %g\n",
				link_sensor->sensor->get_sensor_type().c_str(), link_sensor->coeff, link_sensor->offset);
		}
	}
	thd_log_info("Add virtual sensor success\n");

	return THD_SUCCESS;
}

unsigned int cthd_sensor_virtual::read_temperature() {
	if (polling)
		return last_temp;

	return _read_temperature();
}

unsigned int cthd_sensor_virtual::_read_temperature() {
	double virt_temp = 0.0;
	int temp = 0;

	link_sensor_t *link_sensor = link_sensors[0];

	if (link_sensor && !link_sensor->coeff && !link_sensor->offset) {
		link_sensor_t *link_sensor = link_sensors[0];

		temp = link_sensor->sensor->read_temperature();
		temp = temp * multiplier + offset;
		thd_log_debug("cthd_sensor_virtual::read_temperature %u\n", temp);

		return temp;

	}

	for (unsigned int i = 0; i < link_sensors.size(); ++i) {
		link_sensor_t *link_sensor = link_sensors[i];

		if (link_sensor->sensor) {
			double avg;
			double current_virt_temp;

			temp = link_sensor->sensor->read_temperature();

			if (!link_sensor->power_sensor)
				temp = temp / 1000; //convert to degree C

			if (!link_sensor->prev_avg)
				link_sensor->prev_avg = (double)temp;

			avg = link_sensor->offset * (double) temp  + ((1.0 - link_sensor->offset) * link_sensor->prev_avg);
			current_virt_temp = link_sensor->coeff * avg;
			link_sensor->prev_avg = avg;
			virt_temp += current_virt_temp;
			thd_log_debug("%s-> power[%d] avg[%g] coeff[%g], result[%g] \n", link_sensor->sensor->get_sensor_type().c_str(), temp, avg, current_virt_temp, virt_temp);
		}
	}

	thd_log_debug("virt temp:%g\n", virt_temp);

	temp = static_cast<int>(std::round(virt_temp));

	// always return in mC
	last_temp = temp * 1000;

	return (int) temp;
}

int cthd_sensor_virtual::sensor_update_param(const std::string& new_dep_sensor, double slope, double intercept)
{
	cthd_sensor *sensor = thd_engine->search_sensor(new_dep_sensor);


	if (sensor) {
		link_sensor_t *link_sensor = new (link_sensor_t);

		link_sensor->sensor = sensor;
		link_sensor->offset = 0;
		link_sensor->coeff = 0;
		link_sensor->prev_avg = 0.0;
		link_sensors.push_back(link_sensor);

	} else
		return THD_ERROR;

	multiplier = slope;
	offset = intercept;

	return THD_SUCCESS;
}

void cthd_sensor_virtual::update_polling_table(struct polling_table_entry& entry)
{
		polling_table.push_back(entry);
}

static void *periodic_callback(void *data) {
	cthd_sensor_virtual *sensor = (cthd_sensor_virtual *) data;

	for (;;) {
		sensor->_read_temperature();

		int period = 0;
		for (unsigned int i = sensor->polling_table.size() - 1; i > 0 ; --i) {
				struct polling_table_entry entry = sensor->polling_table[i];

				if (sensor->last_temp >= entry.virtual_temp) {
					period = entry.sample_period;
					break;
				}
		}

		if (period) {
			sensor->polling_period = period;
			thd_log_debug("Update Sample period is set to %d seconds\n", period);
		}

		sleep(sensor->polling_period);
	}

	return NULL;
}

void cthd_sensor_virtual::enable_periodic_timer()
{
	int period = 0;
	int invalid_poll = 100;

	if (polling_table.size() >= 1) {
		if (polling_table[0].sample_period < invalid_poll)
			period = polling_table[0].sample_period;
	}

	if (period) {
		polling_period = period;
		thd_log_info("Sample period is set to %d seconds\n", polling_period);
	}

	pthread_attr_init(&thd_attr);
	pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&poll_thread, &thd_attr, periodic_callback, (void*) this);
	polling = 1;
}
