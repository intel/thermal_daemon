/*
 * thd_default_binding.h: Default binding of  thermal zones
 *	interface file
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
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

#ifndef THD_DEFAULT_BINDING_H_
#define THD_DEFAULT_BINDING_H_

#include "thd_engine.h"
#include "thd_cdev.h"

class cthd_default_binding {
private:
	int thd_read_default_thermal_zones();
	cthd_cdev *cdev_gate;
	unsigned int cpu_package_max_power;

	bool blacklist_match(std::string name);

public:
	static const int def_gating_cdev_sampling_period = 30;
	static const int def_starting_power_differential = 4000000;

	cthd_default_binding() :
			cdev_gate(NULL), cpu_package_max_power(0) {
	}
	~ cthd_default_binding() {
		if (cdev_gate)
			delete cdev_gate;
	}
	void do_default_binding(std::vector<cthd_cdev *> &cdevs);

	bool check_cpu_load();

};

#endif /* THD_DEFAULT_BINDING_H_ */
