/*
 * thd_rapl_power_meter.h: thermal cooling class interface
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

#ifndef THD_CDEV_RAPL_POWER_METER_H_
#define THD_CDEV_RAPL_POWER_METER_H_

#include "thd_common.h"
#include "thd_sys_fs.h"
#include <vector>

typedef enum {
	PACKAGE = 0x01, DRAM = 0x02, CORE = 0x04, UNCORE = 0x08
} domain_type;

typedef struct {
	domain_type type;
	std::string name;
	std::string path;
	// Store in milli-units to have a bigger range
	unsigned long long max_energy_range;
	unsigned long long max_energy_range_threshold;
	int half_way;
	unsigned long long energy_cumulative_counter;
	unsigned long long energy_counter;
	unsigned int power;
	unsigned int max_power;
	unsigned int min_power;
} rapl_domain_t;

class cthd_rapl_power_meter {
private:
	bool rapl_present;
	csys_fs rapl_sysfs;
	std::vector<rapl_domain_t> domain_list;
	time_t last_time;
	pthread_t poll_thread;
	pthread_attr_t thd_attr;
	unsigned int measure_mask;
	bool enable_measurement;

public:
	static const int rapl_callback_timeout = 10; //seconds
	cthd_rapl_power_meter(unsigned int mask = PACKAGE | DRAM);

	void rapl_read_domains(const char *base_path);
	void rapl_enable_periodic_timer();
	bool rapl_energy_loop();
	void rapl_measure_power();
	void rapl_start_measure_power() {
		enable_measurement = true;
	}
	void rapl_stop_measure_power() {
		enable_measurement = false;
	}

	// return in micro units to be compatible with kernel ABI
	unsigned long long rapl_action_get_energy(domain_type type);
	unsigned int rapl_action_get_power(domain_type type);
	unsigned int rapl_action_get_power(domain_type type,
			unsigned int *max_power, unsigned int *min_power);
	unsigned int rapl_action_get_max_power(domain_type type);
};

#endif
