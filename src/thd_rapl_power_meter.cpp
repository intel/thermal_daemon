/*
 * thd_rapl_power_meter.cpp: thermal cooling class implementation
 *	using RAPL
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

#include "thd_rapl_power_meter.h"
#include <dirent.h>
#include <fnmatch.h>
#include <time.h>

static void *rapl_periodic_callback(void *data) {
	cthd_rapl_power_meter *rapl_cl = (cthd_rapl_power_meter*) data;

	for (;;) {
		if (!rapl_cl->rapl_energy_loop())
			break;
		sleep(rapl_cl->rapl_callback_timeout);
	}

	return NULL;
}

cthd_rapl_power_meter::cthd_rapl_power_meter(unsigned int mask) :
		rapl_present(true), rapl_sysfs("/sys/class/powercap/intel-rapl/"), domain_list(
				0), last_time(0), poll_thread(0), measure_mask(mask), enable_measurement(
				false) {
	thd_attr = pthread_attr_t();

	if (rapl_sysfs.exists()) {
		thd_log_debug("RAPL sysfs present\n");
		rapl_present = true;
		last_time = time(NULL);
		rapl_read_domains(rapl_sysfs.get_base_path());
	} else {
		thd_log_warn("NO RAPL sysfs present\n");
		rapl_present = false;
	}
}

void cthd_rapl_power_meter::rapl_read_domains(const char *dir_name) {
	int count = 0;
	csys_fs sys_fs;

	if (rapl_present) {
		DIR *dir;
		struct dirent *dir_entry;
		thd_log_debug("RAPL base path %s\n", dir_name);
		if ((dir = opendir(dir_name)) != NULL) {
			while ((dir_entry = readdir(dir)) != NULL) {
				std::string buffer;
				std::stringstream path;
				int status;
				rapl_domain_t domain;

				domain.half_way = 0;
				domain.energy_counter = 0;
				domain.energy_cumulative_counter = 0;
				domain.max_energy_range = 0;
				domain.max_energy_range_threshold = 0;
				domain.power = 0;
				domain.max_power = 0;
				domain.min_power = 0;
				domain.type = INVALID;

				if (!strcmp(dir_entry->d_name, ".")
						|| !strcmp(dir_entry->d_name, ".."))
					continue;
				thd_log_debug("RAPL domain dir %s\n", dir_entry->d_name);
				path << dir_name << dir_entry->d_name << "/" << "name";
				if (!sys_fs.exists(path.str())) {
					thd_log_debug(" %s doesn't exist\n", path.str().c_str());
					continue;
				}
				status = sys_fs.read(path.str(), buffer);
				if (status < 0)
					continue;
				thd_log_debug("name %s\n", buffer.c_str());
				if (fnmatch("package-*", buffer.c_str(), 0) == 0) {
					domain.type = PACKAGE;
					std::stringstream path;
					path << dir_name << dir_entry->d_name << "/";
					rapl_read_domains(path.str().c_str());
				} else if (buffer == "core") {
					domain.type = CORE;
				} else if (buffer == "uncore") {
					domain.type = UNCORE;
				} else if (buffer == "dram") {
					domain.type = DRAM;
				}
				if (measure_mask & domain.type) {
					domain.name = std::move(buffer);
					domain.path = std::string(dir_name)
							+ std::string(dir_entry->d_name);
					domain_list.push_back(domain);
					++count;
				}
			}
			closedir(dir);
		} else {
			thd_log_debug("opendir failed %s :%s\n", strerror(errno),
					rapl_sysfs.get_base_path());
		}
	}

	thd_log_info("RAPL domain count %d\n", count);
}

void cthd_rapl_power_meter::rapl_enable_periodic_timer() {
	pthread_attr_init(&thd_attr);
	pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&poll_thread, &thd_attr, rapl_periodic_callback,
			(void*) this);
}

bool cthd_rapl_power_meter::rapl_energy_loop() {
	csys_fs sys_fs;
	int status;
	unsigned long long counter;
	unsigned long long diff;
	time_t curr_time;

	if (!enable_measurement)
		return false;

	curr_time = time(NULL);
	if ((curr_time - last_time) <= 0)
		return true;
	for (unsigned int i = 0; i < domain_list.size(); ++i) {
		std::string buffer;
		std::string path;

		if (!domain_list[i].max_energy_range) {
			std::string _path;
			std::string _buffer;
			_path = domain_list[i].path + "/" + "max_energy_range_uj";
			status = sys_fs.read(_path, _buffer);
			if (status >= 0)
				domain_list[i].max_energy_range = atoll(_buffer.c_str()) / 1000;
			domain_list[i].max_energy_range_threshold =
					domain_list[i].max_energy_range / 2;
		}

		path = domain_list[i].path + "/" + "energy_uj";
		status = sys_fs.read(path, buffer);
		if (status >= 0) {
			counter = domain_list[i].energy_counter;
			domain_list[i].energy_counter = atoll(buffer.c_str()) / 1000; // To milli Js

			diff = 0;
			if (domain_list[i].half_way
					&& domain_list[i].energy_counter
							< domain_list[i].max_energy_range_threshold) {
				// wrap around
				domain_list[i].energy_cumulative_counter +=
						domain_list[i].max_energy_range;
				diff = domain_list[i].max_energy_range - counter;
				counter = 0;
				domain_list[i].half_way = 0;

			} else if (domain_list[i].energy_counter
					> domain_list[i].max_energy_range_threshold)
				domain_list[i].half_way = 1;

			if (counter)
				domain_list[i].power = (domain_list[i].energy_counter - counter
						+ diff) / (curr_time - last_time);
			if (domain_list[i].power > domain_list[i].max_power)
				domain_list[i].max_power = domain_list[i].power;

			if (domain_list[i].min_power == 0)
				domain_list[i].min_power = domain_list[i].power;
			else if (domain_list[i].power < domain_list[i].min_power)
				domain_list[i].min_power = domain_list[i].power;

			thd_log_debug(" energy %d:%lld:%lld mj: %u mw\n",
					domain_list[i].type,
					domain_list[i].energy_cumulative_counter,
					domain_list[i].energy_counter
							+ domain_list[i].energy_cumulative_counter,
					domain_list[i].power);

		}
	}
	last_time = curr_time;

	return true;
}

unsigned long long cthd_rapl_power_meter::rapl_action_get_energy(
		domain_type type) {
	unsigned long long value = 0;

	for (unsigned int i = 0; i < domain_list.size(); ++i) {
		if (type == domain_list[i].type) {
			value = (domain_list[i].energy_counter
					+ domain_list[i].energy_cumulative_counter) * 1000;
			if (!value) {
				rapl_energy_loop();
				value = (domain_list[i].energy_counter
						+ domain_list[i].energy_cumulative_counter) * 1000;
			}

			break;
		}
	}

	return value;
}

unsigned int cthd_rapl_power_meter::rapl_action_get_power(domain_type type) {
	unsigned int value = 0;

	if (!rapl_present)
		return 0;

	for (unsigned int i = 0; i < domain_list.size(); ++i) {
		if (type == domain_list[i].type) {
			value = domain_list[i].power * 1000;
			if (!value) {
				rapl_energy_loop();
				sleep(1);
				rapl_energy_loop();
				value = domain_list[i].power * 1000;
			}
			break;
		}
	}

	return value;
}

unsigned int cthd_rapl_power_meter::rapl_action_get_max_power(
		domain_type type) {
	unsigned int value = 0;

	if (!rapl_present)
		return 0;

	for (unsigned int i = 0; i < domain_list.size(); ++i) {
		if (type == domain_list[i].type) {
			int status;
			std::string _path;
			std::string _buffer;
			csys_fs sys_fs;
			unsigned int const_0_val, const_1_val;

			const_0_val = 0;
			const_1_val = 0;
			_path = domain_list[i].path + "/" + "constraint_0_max_power_uw";
			status = sys_fs.read(_path, _buffer);
			if (status >= 0)
				const_0_val = atoi(_buffer.c_str());

			_path = domain_list[i].path + "/" + "constraint_1_max_power_uw";
			status = sys_fs.read(_path, _buffer);
			if (status >= 0)
				const_1_val = atoi(_buffer.c_str());

			value = const_1_val > const_0_val ? const_1_val : const_0_val;
			if (value)
				return value;
		}
	}

	return value;
}

unsigned int cthd_rapl_power_meter::rapl_action_get_power(domain_type type,
		unsigned int *max_power, unsigned int *min_power) {
	unsigned int value = 0;

	if (!rapl_present)
		return 0;

	for (unsigned int i = 0; i < domain_list.size(); ++i) {
		if (type == domain_list[i].type) {
			value = domain_list[i].power * 1000;
			if (!value) {
				rapl_energy_loop();
				sleep(1);
				rapl_energy_loop();
				value = domain_list[i].power * 1000;
			}
			*max_power = domain_list[i].max_power * 1000;
			*min_power = domain_list[i].min_power * 1000;
			break;
		}
	}

	return value;
}

void cthd_rapl_power_meter::rapl_measure_power() {
	if (rapl_present && enable_measurement)
		rapl_energy_loop();
}
