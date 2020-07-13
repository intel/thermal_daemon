/*
 * thd_sensor_rapl_power.h: Power Sensor for rapl
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
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

#ifndef THD_SENSOR_RAPL_POWER_H
#define THD_SENSOR_RAPL_POWER_H

#include "thd_sensor.h"

class cthd_sensor_rapl_power: public cthd_sensor {
private:
public:
	cthd_sensor_rapl_power(int index);
	unsigned int read_temperature();
};

#endif
