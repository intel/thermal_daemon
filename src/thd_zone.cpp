/*
 * thd_zone.cpp: thermal zone class implentation
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

/* This class implements parent thermal zone/sensor. It is included
 * in a thermal engine. During initialization, it establishes a
 * relationship between cooling devices and trip points (where
 * some action needs to be taken).
 * When it gets a notification for a change, it reads the temperature
 * from sensors and uses cthd_trip point to schedule action on the event
 * if required.
 */

#include "thd_zone.h"
#include "thd_engine.h"

cthd_zone::cthd_zone(int _index, std::string control_path, sensor_relate_t rel) :
		index(_index), zone_sysfs(control_path.c_str()), zone_temp(0), zone_active(
				false), zone_cdev_binded_status(false), type_str(), sensor_rel(
				rel), thd_model("") {
	thd_log_debug("Added zone index:%d \n", index);
}

cthd_zone::~cthd_zone() {
	trip_points.clear();
	sensors.clear();
}

void cthd_zone::thermal_zone_temp_change(int id, unsigned int temp, int pref) {
	int i, count;
	bool updated_max = false;
	bool reset = false;

	count = trip_points.size();
	for (i = count - 1; i >= 0; --i) {
		cthd_trip_point &trip_point = trip_points[i];
		if (trip_point.get_trip_type() == MAX) {
			thd_model.add_sample(zone_temp);
			if (thd_model.is_set_point_reached()) {
				int set_point;
				set_point = thd_model.get_set_point();
				thd_log_debug("new set point %d \n", set_point);
				trip_point.thd_trip_update_set_point(set_point);
				updated_max = true;
			}
		}
		trip_point.thd_trip_point_check(id, temp, pref, &reset);
		// Force all cooling devices to min state
		if (reset) {
			zone_reset();
			break;
		}
	}
	// Re-adjust polling thresholds
	if (updated_max) {
		for (i = count - 1; i >= 0; --i) {
			cthd_trip_point &trip_point = trip_points[i];
			if (trip_point.get_trip_type() == POLLING) {
				thd_log_debug("new poll point %d \n",
						thd_model.get_hot_zone_trigger_point());
				trip_point.thd_trip_update_set_point(
						thd_model.get_hot_zone_trigger_point());
				trip_point.thd_trip_point_check(id, temp, pref, &reset);
			}
		}
	}
}

void cthd_zone::update_zone_preference() {
	if (!zone_active)
		return;
	thd_log_debug("update_zone_preference\n");
	thd_model.update_user_set_max_temp();

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		cthd_sensor *sensor;
		sensor = sensors[i];
		zone_temp = sensor->read_temperature();
		thermal_zone_temp_change(sensor->get_index(), 0,
				thd_engine->get_preference());
	}

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		cthd_sensor *sensor;
		sensor = sensors[i];
		zone_temp = sensor->read_temperature();
		thermal_zone_temp_change(sensor->get_index(), zone_temp,
				thd_engine->get_preference());
	}
}

int cthd_zone::read_user_set_psv_temp() {
	std::stringstream filename;
	int temp = -1;

	filename << TDRUNDIR << "/" << "thd_user_psv_temp." << type_str << "."
			<< "conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (ifs.good()) {
		ifs >> temp;
		thd_log_info("read_user_set_psv_temp %d\n", temp);
		if (temp < 1000)
			temp = -1;
	}
	ifs.close();

	return temp;
}

int cthd_zone::zone_update() {
	int ret;

	thd_model.set_zone_type(type_str);
	thd_model.use_pid();

	if (zone_bind_sensors() != THD_SUCCESS) {
		thd_log_warn("Zone update failed: unable to bind \n");
		return THD_ERROR;
	}
	ret = read_trip_points();
	if (ret != THD_SUCCESS)
		return THD_ERROR;

	int usr_psv_temp = read_user_set_psv_temp();
	if (usr_psv_temp > 0) {
		cthd_trip_point trip_pt_passive(0, PASSIVE, usr_psv_temp, 0, index,
				DEFAULT_SENSOR_ID);
		update_trip_temp(trip_pt_passive);
	}

	ret = read_cdev_trip_points();
	if (ret != THD_SUCCESS) {
		thd_log_info("No cdev trip points loaded for zone index %d\n", index);
		// Don't bail out as they may be attached by thermal relation tables
	}

	if (trip_points.size()) {
		unsigned int polling_trip = 0;
		unsigned int max_trip_temp = 0;

		std::sort(trip_points.begin(), trip_points.end(), trip_sort);
		thd_log_info("Sorted trip dump :\n");
		for (unsigned int i = 0; i < trip_points.size(); ++i) {
			trip_points[i].trip_dump();
		}

		// Set the lowest trip point as the threshold for sensor async mode
		// Use that the lowest point, after that we poll
		if (trip_points.size())
			polling_trip = trip_points[0].get_trip_temp();
		for (unsigned int i = 0; i < trip_points.size(); ++i) {
			if (polling_trip > trip_points[i].get_trip_temp())
				polling_trip = trip_points[i].get_trip_temp();
			if (trip_points[i].get_trip_type() == MAX)
				max_trip_temp = trip_points[i].get_trip_temp();
			thd_log_info("trip type: %d temp: %d \n",
					trip_points[i].get_trip_type(),
					trip_points[i].get_trip_temp());
		}
		for (unsigned int i = 0; i < sensors.size(); ++i) {
			cthd_sensor *sensor;
			sensor = sensors[i];
			if (sensor->check_async_capable()) {
				if (max_trip_temp) {
					unsigned int _polling_trip;
					// We have to guarantee MAX, so we better
					// wake up before, so that by the time
					// we are notified, temp > max temp
					thd_model.set_max_temperature(max_trip_temp);
					_polling_trip = thd_model.get_hot_zone_trigger_point();
					if (polling_trip) {
						if (_polling_trip < polling_trip) {
							if ((polling_trip - _polling_trip)
									< def_async_trip_offset)
								polling_trip = _polling_trip
										- def_async_trip_offset;
							else
								polling_trip = _polling_trip;
						}
					} else
						polling_trip = _polling_trip;
				} else {
					if (polling_trip > def_async_trip_offset)
						polling_trip -= def_async_trip_offset;
				}

				sensor->set_threshold(0, polling_trip);
				cthd_trip_point trip_pt_polling(trip_points.size(), POLLING,
						polling_trip, 0, index, sensor->get_index());
				trip_pt_polling.thd_trip_point_set_control_type(PARALLEL);
				trip_points.push_back(trip_pt_polling);
			}
		}
	}

	for (unsigned int i = 0; i < trip_points.size(); ++i) {
		cthd_trip_point &trip_point = trip_points[i];
		unsigned int set_point;
		if (trip_point.get_trip_type() == MAX) {
			thd_model.set_max_temperature(trip_point.get_trip_temp());
			set_point = thd_model.get_set_point();
			if (set_point != thd_model.get_set_point()) {
				trip_point.thd_trip_update_set_point(set_point);
			}
		}
	}

	return THD_SUCCESS;
}

void cthd_zone::read_zone_temp() {
	if (zone_active) {
		unsigned int temp;
		zone_temp = 0;
		for (unsigned int i = 0; i < sensors.size(); ++i) {
			cthd_sensor *sensor;
			sensor = sensors[i];
			temp = sensor->read_temperature();
			if (zone_temp < temp)
				zone_temp = temp;
			if (sensor_rel == SENSOR_INDEPENDENT)
				thermal_zone_temp_change(sensor->get_index(), temp,
						thd_engine->get_preference());
		}
		if (sensor_rel == SENSORS_CORELATED && zone_temp)
			thermal_zone_temp_change(sensors[0]->get_index(), zone_temp,
					thd_engine->get_preference());
	}
}

void cthd_zone::zone_temperature_notification(int type, int data) {
	read_zone_temp();
}

void cthd_zone::zone_reset() {
	int i, count;

	if (zone_active) {
		count = trip_points.size();
		for (i = count - 1; i >= 0; --i) {
			cthd_trip_point &trip_point = trip_points[i];
			trip_point.thd_trip_cdev_state_reset();
		}
	}
}

int cthd_zone::bind_cooling_device(trip_point_type_t type,
		unsigned int trip_temp, cthd_cdev *cdev, int influence,
		int sampling_period) {
	int i, count;
	bool added = false;

	// trip_temp = 0 is a special case, where it will add to first matched type
	count = trip_points.size();
	for (i = 0; i < count; ++i) {
		cthd_trip_point &trip_point = trip_points[i];
		if ((trip_point.get_trip_type() == type)
				&& (trip_point.get_trip_temp() > 0)
				&& (trip_temp == 0 || trip_point.get_trip_temp() == trip_temp)) {
			trip_point.thd_trip_point_add_cdev(*cdev,
					cthd_trip_point::default_influence, sampling_period);
			added = true;
			zone_cdev_set_binded();
			break;
		}
	}
#if 0
	// Check again, if we really need this logic
	if (!added && trip_temp) {
		// Create a new trip point and add only if trip_temp is valid
		cthd_trip_point trip_pt(count, type, trip_temp, 0, index,
				DEFAULT_SENSOR_ID);
		trip_points.push_back(trip_pt);
		added = true;
	}
#endif
	if (added)
		return THD_SUCCESS;
	else
		return THD_ERROR;
}

int cthd_zone::update_max_temperature(int max_temp) {

	std::stringstream filename;
	std::stringstream temp_str;

	filename << TDRUNDIR << "/" << "thd_user_set_max." << type_str << "."
			<< "conf";
	std::ofstream fout(filename.str().c_str());
	if (!fout.good()) {
		return THD_ERROR;
	}
	temp_str << max_temp;
	fout << temp_str.str();
	fout.close();

	return THD_SUCCESS;
}

int cthd_zone::update_psv_temperature(int psv_temp) {

	std::stringstream filename;
	std::stringstream temp_str;

	filename << TDRUNDIR << "/" << "thd_user_psv_temp." << type_str << "."
			<< "conf";
	std::ofstream fout(filename.str().c_str());
	if (!fout.good()) {
		return THD_ERROR;
	}
	temp_str << psv_temp;
	fout << temp_str.str();
	fout.close();

	return THD_SUCCESS;
}

void cthd_zone::add_trip(cthd_trip_point &trip) {
	bool add = true;
	for (unsigned int j = 0; j < trip_points.size(); ++j) {
		if (trip_points[j].get_trip_type() == trip.get_trip_type()) {
			thd_log_debug("updating existing trip temp \n");
			trip_points[j] = trip;
			if (trip.get_trip_type() == MAX) {
				thd_model.set_max_temperature(trip.get_trip_temp());
				// TODO: If sensor supports polling
				// update the polling threshold also.
			}
			add = false;
			break;
		}
	}
	if (add)
		trip_points.push_back(trip);
}

void cthd_zone::update_trip_temp(cthd_trip_point &trip) {
	for (unsigned int j = 0; j < trip_points.size(); ++j) {
		if (trip_points[j].get_trip_type() == trip.get_trip_type()) {
			thd_log_debug("updating existing trip temp \n");
			trip_points[j].update_trip_temp(trip.get_trip_temp());
			trip_points[j].update_trip_hyst(trip.get_trip_hyst());
			if (trip.get_trip_type() == MAX) {
				thd_model.set_max_temperature(trip.get_trip_temp());
				// TODO: If sensor supports polling
				// update the polling threshold also.
			}
			break;
		}
	}
}
