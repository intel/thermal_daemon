/*
 * thd_sensor.h: thermal sensor class interface
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

#ifndef THD_SENSOR_H_
#define THD_SENSOR_H_

#include <vector>
#include "thd_common.h"
#include "thd_sys_fs.h"

#define SENSOR_TYPE_THERMAL_SYSFS	0
#define SENSOR_TYPE_RAW				1

class cthd_sensor {
protected:
	int index;
	int type;
	csys_fs sensor_sysfs;
	bool sensor_active;
	std::string type_str;
	bool async_capable;
	bool virtual_sensor;

private:
	std::vector<int> thresholds;
	int scale;

	void enable_uevent();

public:
	cthd_sensor(int _index, std::string control_path, std::string _type_str,
			int _type = SENSOR_TYPE_THERMAL_SYSFS);
	virtual ~cthd_sensor() {
	}
	int sensor_update();
	virtual std::string get_sensor_type() {
		return type_str;
	}

	virtual std::string get_sensor_path() {
		return sensor_sysfs.get_base_path();
	}

	virtual unsigned int read_temperature();
	int get_index() {
		return index;
	}
	int set_threshold(int index, int temp);
	;
	void update_path(std::string str) {
		sensor_sysfs.update_path(std::move(str));
	}
	void set_async_capable(bool capable) {
		async_capable = capable;
	}
	bool check_async_capable() {
		return async_capable;
	}
	void set_scale(int _scale) {
		scale = _scale;
	}
	virtual void sensor_dump() {
		thd_log_info("sensor index:%d %s %s Async:%d\n", index,
				type_str.c_str(), sensor_sysfs.get_base_path(), async_capable);
	}
	// Even if sensors are capable of async, it is possible that it is not reliable enough
	// at critical monitoring point. Sensors can be forced to go to poll mode at that temp
	void sensor_poll_trip(bool status);

	void sensor_fast_poll(bool status);

	bool is_virtual() {
		return virtual_sensor;
	}
};

#endif /* THD_SENSOR_H_ */
