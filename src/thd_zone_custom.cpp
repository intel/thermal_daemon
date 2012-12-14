/*
 * thd_zone_custom.h: thermal engine custom class implementation
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
 * This implementation allows to overide per zone read data from
 * sysfs for buggy BIOS. 
 */

#include "thd_zone_custom.h"

int cthd_zone_custom::read_trip_points()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_zone::read_trip_points();
}

int cthd_zone_custom::read_cdev_trip_points()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_zone::read_cdev_trip_points();
}


