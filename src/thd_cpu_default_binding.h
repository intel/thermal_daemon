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

typedef struct {
	char zone_name[50 + 1];
	int failures;
} cpu_zone_stat_t;

typedef struct {
	std::string zone_name;
	cthd_cdev *cdev_gate_entry;
	cthd_cdev *cdev_gate_exit;
	cthd_zone *zone;
} cpu_zone_binding_t;

class cthd_cpu_default_binding {
private:
	int thd_read_default_thermal_zones();
	unsigned int cpu_package_max_power;

	bool blacklist_match(std::string name);

public:
	static const int def_gating_cdev_sampling_period = 30;
	static const unsigned int def_starting_power_differential = 4000000;

	std::vector<cpu_zone_binding_t*> cdev_list;

	cthd_cpu_default_binding() :
			cpu_package_max_power(0) {
	}
	~ cthd_cpu_default_binding() {
		for (unsigned int i = 0; i < cdev_list.size(); ++i) {
			cpu_zone_binding_t *cdev_binding_info = cdev_list[i];
			delete cdev_binding_info->cdev_gate_exit;
			delete cdev_binding_info->cdev_gate_entry;
			delete cdev_binding_info;
		}
		cdev_list.clear();
	}

	void do_default_binding(std::vector<cthd_cdev *> &cdevs);

	bool check_cpu_load();

	int read_zone_stat(std::string zone_name, cpu_zone_stat_t *stat);
	void update_zone_stat(std::string zone_name, int fail_cnt);

};

#endif /* THD_DEFAULT_BINDING_H_ */
