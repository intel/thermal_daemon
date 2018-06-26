/*
 * thd_zone_kbl_amdgpu.h: thermal zone for KBL-G amdgpu
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

#ifndef THD_ZONE_KBL_G_MCP_H
#define THD_ZONE_KBL_G_MCP_H

#include "thd_zone.h"

class cthd_zone_kbl_g_mcp: public cthd_zone {
protected:
	cthd_sensor *sensor;
	csys_fs dts_sysfs;
public:
	cthd_zone_kbl_g_mcp(int index);
	int read_trip_points();
	int zone_bind_sensors();
	int read_cdev_trip_points();
};

#endif
