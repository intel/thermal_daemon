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

/*
 * It will search for unbounded zones (There is valid passive trips and temp
 * reading is available but there is no cooling driver attached to it via XML
 * config or default hardcoded config.
 * In this case if CPU is causing this temp to go up then it will use RAPL and
 * power clamp to cool. If it fails for three times then this zone is added
 * to black list, so that this method will never be tried again,
 * The way this file implements using existing mechanism via a two pseudo
 * cooling devices "gates" called start and exit. The start gate checks the
 * current cpu load, if yes then it opens gate by changing it to max state.
 * Similarly if end gate state is reached means that the RAPL and power clamp
 * failed to control temperature of this zone.
 */

#include "thd_zone_therm_sys_fs.h"

#include "thd_cpu_default_binding.h"

class cthd_gating_cdev: public cthd_cdev {
private:
	class cthd_cpu_default_binding *def_bind_ref;
	cpu_zone_binding_t *bind_zone;
	bool start;

public:
	static const int max_state = 0x01;
	cthd_gating_cdev(int _id, cthd_cpu_default_binding *_def_bind_ref,
			cpu_zone_binding_t *_bind_zone, bool _start) :
			cthd_cdev(_id, ""), def_bind_ref(_def_bind_ref), bind_zone(
					_bind_zone), start(_start) {
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

	if (!start && state) {
		if (!bind_zone->zone)
			return;
		thd_log_info("CPU def binding exit for %s\n",
				bind_zone->zone->get_zone_type().c_str());
		bind_zone->zone->zone_reset();

		cpu_zone_stat_t stats;
		if (def_bind_ref->read_zone_stat(bind_zone->zone->get_zone_type(),
				&stats) == THD_SUCCESS) {
			++stats.failures;
			def_bind_ref->update_zone_stat(bind_zone->zone->get_zone_type(),
					stats.failures);
			if (stats.failures > 3) {
				// This zone can't be controlled by this binding
				thd_log_info("CPU def binding is set to inactive for %s\n",
						bind_zone->zone->get_zone_type().c_str());
				bind_zone->zone->set_zone_inactive();
			}
		} else {
			def_bind_ref->update_zone_stat(bind_zone->zone->get_zone_type(), 0);
		}
	} else if (start && state) {
		if (def_bind_ref->check_cpu_load()) {
			thd_log_info("Turn on the gate\n");
			curr_state = max_state;
		} else {
			thd_log_info("Not CPU specific increase\n");
			curr_state = 0;
		}
	} else {
		curr_state = 0;
	}

	thd_log_info(
			"cthd_gating_cdev::set_curr_state[start:%d state:%d. curr_state:%d\n",
			start, state, curr_state);
}

int cthd_gating_cdev::get_max_state() {
	return max_state;
}

int cthd_gating_cdev::update() {
	thd_log_info("cthd_gating_cdev::update\n");

	return 0;
}

int cthd_cpu_default_binding::read_zone_stat(std::string zone_name,
		cpu_zone_stat_t *stat) {
	std::ifstream filein;

	std::stringstream filename;
	filename << TDRUNDIR << "/" << "cpu_def_zone_bind.out";

	filein.open(filename.str().c_str(), std::ios::in | std::ios::binary);
	if (!filein)
		return THD_ERROR;

	while (filein) {
		filein.read((char *) stat, sizeof(*stat));
		thd_log_info("read_zone_stat name:%s f:%d\n", stat->zone_name,
				stat->failures);
		if (filein && zone_name == stat->zone_name)
			return THD_SUCCESS;
	}

	filein.close();

	return THD_ERROR;
}

void cthd_cpu_default_binding::update_zone_stat(std::string zone_name,
		int fail_cnt) {
	std::ifstream filein;
	std::streampos current = 0;
	cpu_zone_stat_t obj;
	bool found = false;

	std::stringstream filename;
	filename << TDRUNDIR << "/" << "cpu_def_zone_bind.out";

	filein.open(filename.str().c_str(), std::ios::in | std::ios::binary);
	if (filein.is_open()) {
		while (!filein.eof()) {
			current = filein.tellg();
			filein.read((char *) &obj, sizeof(obj));
			if (filein && zone_name == obj.zone_name) {
				found = true;
				break;
			}
		}
	}
	filein.close();

	std::fstream fileout(filename.str().c_str(),
			std::ios::out | std::ios::in | std::ios::binary);

	if (!fileout.is_open()) {
		std::ofstream file;
		file.open(filename.str().c_str());
		if (file.is_open()) {
			file.close();
			fileout.open(filename.str().c_str(),
					std::ios::in | std::ios::out | std::ios::binary);
		} else {
			thd_log_info("Can't create cpu_def_zone_bind.out\n");
		}
	}

	if (found) {
		fileout.seekp(current);
	} else
		fileout.seekp(0, std::ios::end);

	strncpy(obj.zone_name, zone_name.c_str(), 50);
	obj.failures = fail_cnt;
	fileout.write((char *) &obj, sizeof(obj));
	fileout.close();
}

bool cthd_cpu_default_binding::check_cpu_load() {
	unsigned int max_power = 0;
	unsigned int min_power = 0;
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
		thd_log_info("Significant cpu load\n");
		return true;
	}

	return false;
}

bool cthd_cpu_default_binding::blacklist_match(std::string name) {
	int i = 0;
	const char *blacklist_zones[] = { "cpu", "acpitz", "Surface", "pkg-temp-0",
			"x86_pkg_temp", "soc_dts0", "soc_dts1", "B0D4", "B0DB", "" };

	while (blacklist_zones[i][0] != '\0') {
		if (name == blacklist_zones[i])
			return true;
		++i;
	}

	cpu_zone_stat_t stats;
	if (read_zone_stat(name, &stats) == THD_SUCCESS) {
		if (stats.failures > 3) {
			// This zone can't be controlled by this binding
			thd_log_info(" zone %s in blacklist\n", name.c_str());
			return true;
		}
	}

	return false;
}

void cthd_cpu_default_binding::do_default_binding(
		std::vector<cthd_cdev *> &cdevs) {
	int count = 0;
	int id = 0x1000;
	cthd_cdev *cdev_rapl;
	cthd_cdev *cdev_powerclamp;

	cdev_rapl = thd_engine->search_cdev("rapl_controller");
	cdev_powerclamp = thd_engine->search_cdev("intel_powerclamp");

	if (!cdev_rapl && !cdev_powerclamp) {
		thd_log_info(
				"cthd_cpu_default_binding::do_default_binding: No relevant cpu cdevs\n");
		return;
	}

	for (unsigned int i = 0; i < thd_engine->get_zone_count(); ++i) {
		cthd_zone *zone = thd_engine->get_zone(i);

		if (!zone) {
			continue;
		}

		if (blacklist_match(zone->get_zone_type())) {
			continue;
		}
		if (!zone->zone_cdev_binded()) {
			cpu_zone_binding_t *cdev_binding_info;

			cdev_binding_info = new cpu_zone_binding_t;

			cdev_binding_info->zone_name = zone->get_zone_type();
			cdev_binding_info->zone = zone;

			cdev_binding_info->cdev_gate_entry = new cthd_gating_cdev(id++,
					this, cdev_binding_info, true);
			if (cdev_binding_info->cdev_gate_entry) {
				cdev_binding_info->cdev_gate_entry->set_cdev_type(
						cdev_binding_info->zone_name + "_" + "cpu_gate_entry");
			} else {
				thd_log_info("do_default_binding failed\n");
				cdev_binding_info->cdev_gate_entry = NULL;
				delete cdev_binding_info;
				continue;
			}

			cdev_binding_info->cdev_gate_exit = new cthd_gating_cdev(id++, this,
					cdev_binding_info, false);
			if (cdev_binding_info->cdev_gate_exit) {
				cdev_binding_info->cdev_gate_exit->set_cdev_type(
						cdev_binding_info->zone_name + "_" + "cpu_gate_exit");
			} else {
				thd_log_info("do_default_binding failed\n");
				delete cdev_binding_info->cdev_gate_entry;
				cdev_binding_info->cdev_gate_entry = NULL;
				delete cdev_binding_info;
				return;
			}

			thd_log_info("unbound zone %s\n", zone->get_zone_type().c_str());
			int status = zone->bind_cooling_device(PASSIVE, 0,
					cdev_binding_info->cdev_gate_entry, 0,
					def_gating_cdev_sampling_period);
			if (status == THD_ERROR) {
				thd_log_info("unbound zone: Bind attempt failed\n");
				delete cdev_binding_info->cdev_gate_exit;
				cdev_binding_info->cdev_gate_exit = NULL;
				delete cdev_binding_info->cdev_gate_entry;
				cdev_binding_info->cdev_gate_entry = NULL;
				delete cdev_binding_info;
				continue;
			}

			if (cdev_rapl) {
				zone->bind_cooling_device(PASSIVE, 0, cdev_rapl, 20);
			}
			if (cdev_powerclamp) {
				zone->bind_cooling_device(PASSIVE, 0, cdev_powerclamp, 20);
			}

			status = zone->bind_cooling_device(PASSIVE, 0,
					cdev_binding_info->cdev_gate_exit, 0,
					def_gating_cdev_sampling_period);
			if (status == THD_ERROR) {
				thd_log_info("unbound zone: Bind attempt failed\n");
				delete cdev_binding_info->cdev_gate_exit;
				cdev_binding_info->cdev_gate_exit = NULL;
				delete cdev_binding_info->cdev_gate_entry;
				cdev_binding_info->cdev_gate_entry = NULL;
				delete cdev_binding_info;
				continue;
			}
			thd_log_info("unbound zone %s\n", zone->get_zone_type().c_str());

			count++;
			zone->set_zone_active();
			cdev_list.push_back(cdev_binding_info);
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
