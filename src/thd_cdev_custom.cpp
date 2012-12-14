/*
 * thd_cdev_custom.cpp: thermal engine custom cdev class implementation
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
 * This implementation allows to overide per cdev read data from sysfs for buggy BIOS. 
 */

#include "thd_cdev_custom.h"

void cthd_cdev_custom::set_curr_state(int state)
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_cdev::set_curr_state(state);
}

int cthd_cdev_custom::get_curr_state()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_cdev::get_curr_state();
}

int cthd_cdev_custom::get_max_state()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_cdev::get_max_state();
}

int cthd_cdev_custom::update()
{
	// Currently there is no custom interface
	// use default
	thd_log_debug("Use parent method \n");
	return cthd_cdev::update();
}

