/*
 * thd_sensor_kbl_amdgpu_thermal.h: Thermal Sensor for KBL-G amdgpu
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

#ifndef THD_SENSOR_KBL_AMD_GPU_THERMAL_H
#define THD_SENSOR_KBL_AMD_GPU_THERMAL_H

#include "thd_sensor.h"

class cthd_sensor_kbl_amdgpu_thermal: public cthd_sensor {
private:
public:
	cthd_sensor_kbl_amdgpu_thermal(int index);
};

#endif
