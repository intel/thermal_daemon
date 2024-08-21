/*
 * thd_zone.h: thermal zone class interface
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

#ifndef THD_ZONE_H
#define THD_ZONE_H

#include <vector>

#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_cdev.h"
#include "thd_trip_point.h"
#include "thd_sensor.h"

typedef struct {
	int zone;
	int type;
	unsigned int data;
} thermal_zone_notify_t;

// If the zone has multiple sensors, there are two possibilities
// Either the are independent, means that each has own set of trip points
// Or related. In this case one trip point. Here we take max of the sensor reading
// and then apply trip
typedef enum {
	SENSOR_INDEPENDENT, SENSORS_CORELATED
} sensor_relate_t;

class cthd_zone {

protected:
	int index;
	std::vector<cthd_trip_point> trip_points;
	std::string temperature_sysfs_path;
	csys_fs zone_sysfs;
	unsigned int zone_temp;
	bool zone_active;
	bool zone_cdev_binded_status;
	std::string type_str;
	std::vector<cthd_sensor *> sensors;
	sensor_relate_t sensor_rel;

	virtual int zone_bind_sensors() = 0;
	void thermal_zone_temp_change(int id, unsigned int temp, int pref);

private:
	void sort_and_update_poll_trip();
public:
	static const unsigned int def_async_trip_offset = 5000;
	static const unsigned int def_async_trip_offset_pct = 10;

	cthd_zone(int _index, std::string control_path, sensor_relate_t rel =
			SENSOR_INDEPENDENT);
	virtual ~cthd_zone();
	void zone_temperature_notification(int type, int data);
	int zone_update();
	virtual void update_zone_preference();
	void zone_reset(int force = 0);

	virtual int read_trip_points() = 0;
	virtual int read_cdev_trip_points() = 0;
	virtual void read_zone_temp();

	int get_zone_index() {
		return index;
	}

	void add_trip(cthd_trip_point &trip, int force = 0);
	void update_trip_temp(cthd_trip_point &trip);
	void update_highest_trip_temp(cthd_trip_point &trip);

	void set_zone_active() {
		zone_active = true;
	}
	;
	void set_zone_inactive() {
		zone_active = false;
	}

	bool zone_active_status() {
		return zone_active;
	}

	bool zone_cdev_binded() {
		return zone_cdev_binded_status;
	}

	void zone_cdev_set_binded() {
		thd_log_debug("zone %s bounded\n", type_str.c_str());
		zone_cdev_binded_status = true;
	}

	std::string get_zone_type() {
		return type_str;
	}

	std::string get_zone_path() {
		return zone_sysfs.get_base_path();
	}

	void set_zone_type(std::string type) {
		type_str = type;
	}

	void bind_sensor(cthd_sensor *sensor) {
		for (unsigned int i = 0; i < sensors.size(); ++i) {
			if (sensors[i] == sensor)
				return;
		}
		sensors.push_back(sensor);
	}

	// Even if one sensor, it is using doesn't
	// provide async control, return false
	bool check_sensor_async_status() {
		for (unsigned int i = 0; i < sensors.size(); ++i) {
			cthd_sensor *sensor = sensors[i];
			if (!sensor->check_async_capable()) {
				return false;
			}
		}

		return true;
	}

	unsigned int get_trip_count() {
		return trip_points.size();
	}

	int update_max_temperature(int max_temp);
	int update_psv_temperature(int psv_temp);
	int read_user_set_psv_temp();

	int bind_cooling_device(trip_point_type_t type, unsigned int trip_temp,
			cthd_cdev *cdev, int influence, int sampling_period = 0,
			int target_state_valid = 0, int target_state = 0);

	int get_sensor_count() {
		return sensors.size();
	}

	cthd_sensor *get_sensor_at_index(unsigned int index) {
		if (index < sensors.size())
			return sensors[index];
		else
			return NULL;
	}

	cthd_trip_point *get_trip_at_index(unsigned int index) {
		if (index < trip_points.size())
			return &trip_points[index];
		else
			return NULL;
	}
#ifdef ANDROID
	void trip_delete_all() {
		if (!trip_points.size())
			return;

		for (unsigned int i = 0; i < trip_points.size(); ++i) {
			trip_point_type_t trip_type = trip_points[i].get_trip_type();
			if (trip_type==HOT|| trip_type==CRITICAL || trip_type==MAX )
			{
				thd_log_info("keep cdev trip_point %d temp=%d\n", i, trip_points[i].get_trip_temp());
			}
			else
				trip_points[i].delete_cdevs();
		}
		std::vector<cthd_trip_point>::iterator it;
		for (it=trip_points.begin(); it!=trip_points.end(); )
		{
			trip_point_type_t trip_type = it->get_trip_type();
			if (trip_type==HOT|| trip_type==CRITICAL || trip_type==MAX  )
			{
				thd_log_info("keep trip_point  temp=%d\n",  it->get_trip_temp());
				++it;
			}
			else {
				thd_log_info("remove trip_point  temp=%d\n",  it->get_trip_temp());
				it=trip_points.erase(it);
			}
		}
	}
#else
	void trip_delete_all() {
		if (!trip_points.size())
			return;

		for (unsigned int i = 0; i < trip_points.size(); ++i) {
			trip_points[i].delete_cdevs();
		}
		trip_points.clear();
	}
#endif

	void zone_dump() {
		if (!zone_active)
			return;

		thd_log_info("\n");
		thd_log_info("Zone %d: %s, Active:%d Bind:%d Sensor_cnt:%lu\n", index,
				type_str.c_str(), zone_active, zone_cdev_binded_status,
				(unsigned long) sensors.size());
		thd_log_info("..sensors..\n");
		for (unsigned int i = 0; i < sensors.size(); ++i) {
			sensors[i]->sensor_dump();
		}
		thd_log_info("..trips..\n");
		for (unsigned int i = 0; i < trip_points.size(); ++i) {
			trip_points[i].trip_dump();
		}
		thd_log_info("\n");

	}

	;
};

#endif
