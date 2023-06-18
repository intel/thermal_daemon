/*
 * thd_zone_dts.h: thermal engine DTS class interface
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
 * This interface allows overriding per zone read data from sysfs for
 * buggy BIOS.
 */

#ifndef THD_ZONE_DTS_H
#define THD_ZONE_DTS_H

#include "thd_zone.h"
#include <vector>

class cthd_zone_cpu: public cthd_zone {
protected:
	csys_fs dts_sysfs;
	int critical_temp;
	int max_temp;
	int psv_temp;
	int trip_point_cnt;
	unsigned int sensor_mask;
	int phy_package_id;

	std::vector<std::string> sensor_sysfs;

	int init();

	int parse_cdev_order();
	int pkg_thres_th_zone;
	bool pkg_temp_poll_enable;

public:
	static const int max_dts_sensors = 16;
	static const int def_hystersis = 0;
	static const int def_offset_from_critical = 10000;
	static const int def_critical_temp = 100000;

	cthd_zone_cpu(int count, std::string path, int package_id);
	int load_cdev_xml(cthd_trip_point &trip_pt, std::vector<std::string> &list);

	virtual int read_trip_points();
	int read_cdev_trip_points();
	int zone_bind_sensors();

};

#endif
