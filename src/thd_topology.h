/*
 * thd_topology.h: thermal dts sensor topology class interface
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
 */

#ifndef THD_TOPOLOGY_H_
#define THD_TOPOLOGY_H_

#include "thd_sys_fs.h"
#include "thd_msr.h"
#include <vector>
#include <pthread.h>

#define mov_average_window	15

typedef struct {
	unsigned long long mperf;
	unsigned long long tsc;
}per_cpu_perf_data_t;

typedef struct {
	unsigned int samples[mov_average_window];
	int num_samples;
	unsigned int current_total;
	int last_moving_average;
}movinng_average_t;

typedef struct {
	int sensor_id;
}cpu_sensor_relate_t;

typedef struct {
	int cpu_id;
}sensor_cpu_relate_t;

class cthd_topology
{

private:
	static const int measurment_time = mov_average_window;
	cthd_msr	msr;
	int 		no_cpu;
	int			no_sensors;
	per_cpu_perf_data_t *perf_data;

	movinng_average_t *temp_data;
	movinng_average_t *perf_samples;
	cpu_sensor_relate_t *cpu_sensor_rel;
	sensor_cpu_relate_t *sensor_cpu_rel;

	unsigned long long rdtsc();
	int check_temperature(int index);
	float get_c0_present_value(int cpu);

	void add(movinng_average_t *pdata, int index, unsigned int value)
	{
		if (pdata[index].num_samples < mov_average_window) {
			pdata[index].samples[pdata[index].num_samples++] = value;
			pdata[index].current_total += value;
		} else {
			unsigned int& oldest = pdata[index].samples[pdata[index].num_samples++ % mov_average_window];
			pdata[index].current_total += value - oldest;
			oldest = value;
		}
	}

	unsigned int moving_average(movinng_average_t *pdata, int index, unsigned int value)
	{
		add(pdata, index, value);
		return pdata[index].current_total / std::min(pdata[index].num_samples, mov_average_window);
	}
	void store_configuration(int index, unsigned int mask);

public:
	cthd_topology();
	~cthd_topology();

	int calibrate();

	// For running a sample load and get sensor and C0 data
	pthread_t load_thread;
	pthread_t temp_thread;
	pthread_t cal_engine;
	pthread_attr_t thd_attr;
	int		designated_cpu;
	bool terminate_thread;
	bool cal_failed;
	void test_function(cthd_topology *obj);
	void temp_function();
	bool check_load_for_calibration(unsigned char cpu_mask);
	void configuration_saved();
	bool check_config_saved();
};

#endif /* THD_TOPOLOGY_H_ */
