/*
 * thd_default_binding.cpp: Default binding of  thermal zones
 *	implementation file
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
 * Author Name <Srinivas.Pandruvada@linux.intel.com>
 *
 */

#include "thd_zone_therm_sys_fs.h"

#include "thd_default_binding.h"

class cthd_gating_cdev: public cthd_cdev {
private:
	class cthd_default_binding *def_bind_ref;

public:
	static const int max_state = 0xffff;
	cthd_gating_cdev(cthd_default_binding *_def_bind_ref) :
			cthd_cdev(0x1000, ""), def_bind_ref(_def_bind_ref) {
	}
	void set_curr_state(int state, int arg);
	int get_max_state();
	int update();
	int get_curr_state();
};

int cthd_gating_cdev::get_curr_state() {
	return curr_state;
}

void cthd_gating_cdev::set_curr_state(int state, int arg) {

	if (def_bind_ref->check_cpu_load()) {
		thd_log_info("Turn on the gate \n");
		curr_state = max_state;
	} else
		curr_state = 0;

	thd_log_info("cthd_gating_cdev::set_curr_state\n");
}

int cthd_gating_cdev::get_max_state() {
	return max_state;
}

int cthd_gating_cdev::update() {
	thd_log_info("cthd_gating_cdev::update\n");

	return 0;
}

bool cthd_default_binding::check_cpu_load() {
	unsigned int max_power;
	unsigned int min_power;
	unsigned int power;

	power = thd_engine->rapl_power_meter.rapl_action_get_power(PACKAGE,
			&max_power, &min_power);

	if (cpu_package_max_power != 0)
		max_power = cpu_package_max_power;

	thd_log_info("cthd_gating_cdev power :%u %u %u\n", power, min_power,
			max_power);

	if ((max_power - min_power) < def_starting_power_differential) {
		return false;
	}

	if (power > (max_power * 60 / 100)) {
		thd_log_info("Significant cpu load \n");
		return true;
	}

	return false;
}

bool cthd_default_binding::blacklist_match(std::string name) {
	int i = 0;
	const char *blacklist_zones[] = {"cpu", "acpitz", ""};

	while (blacklist_zones[i] != "") {
		if (name == blacklist_zones[i])
			return true;
		++i;
	}

	return false;
}

void cthd_default_binding::do_default_binding(std::vector<cthd_cdev *> &cdevs) {
	int count = 0;

	cdev_gate = new cthd_gating_cdev(this);
	if (cdev_gate) {
		cdev_gate->set_cdev_type("cdev_gate");
	} else {
		thd_log_info("do_default_binding failed \n");
		delete cdev_gate;
		return;
	}

	for (unsigned int i = 0; i < thd_engine->get_zone_count(); ++i) {
		cthd_zone *zone = thd_engine->get_zone(i);

		if (blacklist_match(zone->get_zone_type())) {
			continue;
		}
		if (!zone->read_cdev_trip_points()) {
			thd_log_info("unbound zone %s\n", zone->get_zone_type().c_str());
			int status = zone->bind_cooling_device(PASSIVE, 0, cdev_gate, 0,
					def_gating_cdev_sampling_period);
			if (status == THD_ERROR) {
				thd_log_info("unbound zone: Bind attempt failed\n");
				continue;
			}
			// Currently only considering CPU/SOC load is primary
			cthd_cdev *cdev_rapl;
			cdev_rapl = thd_engine->search_cdev("rapl_controller");
			if (cdev_rapl) {
				zone->bind_cooling_device(PASSIVE, 0, cdev_rapl, 0);
			}
			count++;
			zone->set_zone_active();
		}
	}
	if (count) {
		thd_engine->rapl_power_meter.rapl_start_measure_power();
		cpu_package_max_power =
				thd_engine->rapl_power_meter.rapl_action_get_max_power(PACKAGE);
		thd_log_info("do_default_binding  max power CPU package :%u\n",
				cpu_package_max_power);
	}
}
