/*
 * thd_topology.cpp: thermal dts sensor topology class implementation
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
 */
#include "thd_topology.h"


//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sched.h>

#include <sys/syscall.h>

#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>

// Some test code to run a single CPU 99%
struct dummy
{
	int x;
	int y;

};
static volatile struct dummy dymmy_var;

pid_t gettid(void)
{
	return syscall(__NR_gettid);
} 

void cthd_topology::test_function(cthd_topology *obj)
{
	for(;;)
	{
		if(obj->terminate_thread)
			break;
		__sync_fetch_and_add(&dymmy_var.x, 1);
	}
}

static void *load_thread_loop(void *arg)
{
	cthd_topology *obj = (cthd_topology*)arg;
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(obj->designated_cpu, &set);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if(sched_setaffinity(gettid(), sizeof(cpu_set_t), &set))
	{
		perror("sched_setaffinity");
		return NULL;
	}
	obj->test_function(obj);

	return NULL;
}

void cthd_topology::temp_function()
{
	int i;

	// core temp sensor index starts at 2
	for(i = 2; i < no_sensors; ++i)
	{
		check_temperature(i);
	}
}

static void *temp_thread_loop(void *arg)
{
	cthd_topology *obj = (cthd_topology*)arg;
	cpu_set_t set;
	unsigned char cpu_mask = 0xff;

	cpu_mask &= ~(1 << obj->designated_cpu);
	CPU_ZERO(&set);
	CPU_SET(obj->designated_cpu, &set);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if(sched_setaffinity(gettid(), sizeof(cpu_set_t), &set))
	{
		perror("sched_setaffinity");
		return NULL;
	}
	obj->cal_failed = false;
	for(;;)
	{
		if(obj->terminate_thread)
			break;
		sleep(1);
		if(obj->check_load_for_calibration(cpu_mask) == false)
		{
			obj->cal_failed = true;
			obj->terminate_thread = true;
			thd_log_warn("!unsuitable CPU load \n");
			break;
		}
		obj->temp_function();
	}
	return NULL;
}

cthd_topology::cthd_topology()
{
	no_cpu = msr.get_no_cpus();
	no_sensors = no_cpu + 2; // Just for safety
	perf_data = new per_cpu_perf_data_t[no_cpu];
	memset(perf_data, 0, sizeof(*perf_data) * no_cpu);
	temp_data = new movinng_average_t[no_sensors];
	memset(temp_data, 0, sizeof(*temp_data) * no_sensors);
	perf_samples = new movinng_average_t[no_cpu];
	memset(perf_samples, 0, sizeof(*perf_samples) * no_cpu);
	cpu_sensor_rel = new cpu_sensor_relate_t[no_cpu];
	for(int i = 0; i < no_cpu; ++i)
	{
		cpu_sensor_rel[i].sensor_id =  - 1;
	}

	sensor_cpu_rel = new sensor_cpu_relate_t[no_sensors];
	for(int i = 0; i < no_sensors; ++i)
	{
		sensor_cpu_rel[i].cpu_id =  - 1;
	}
}

cthd_topology::~cthd_topology()
{
	delete[] temp_data;
	delete[] perf_samples;
	delete[] perf_data;
	delete[] cpu_sensor_rel;
	delete[] sensor_cpu_rel;
}

unsigned long long cthd_topology::rdtsc(void)
{
	unsigned int low, high;

	asm volatile("rdtsc": "=a"(low), "=d"(high));

	return low | ((unsigned long long)high) << 32;
}

int cthd_topology::check_temperature(int index)
{
	ssize_t retval;
	char pathname[64];
	char temp_str[10];
	int fd;
	unsigned int temperature;
	int current_moving_average;

	if(index >= no_sensors)
		return 0;
	sprintf(pathname, "/sys/devices/platform/coretemp.0/temp%d_input", index);
	fd = open(pathname, O_RDONLY);
	if(fd < 0)
		return  - 1;
	retval = pread(fd, temp_str, sizeof(temp_str), 0);
	if (retval < 0)
	{
		close(fd);
		return 0;
	}
	temperature = atoi(temp_str);

	current_moving_average = moving_average(temp_data, index, temperature);
	temp_data[index].last_moving_average = current_moving_average;

	thd_log_debug("DTS sensor %d mov average %u \n", index, current_moving_average)
	;
	close(fd);

	return temperature;
}

float cthd_topology::get_c0_present_value(int cpu)
{
	unsigned long long mperf, delta_mperf;
	unsigned long long tsc, delta_tsc;
	float result;
	int current_moving_average;

	tsc = rdtsc();
	msr.get_mperf_value(cpu, &mperf);

	if(!perf_data[cpu].mperf || !perf_data[cpu].tsc)
	{
		perf_data[cpu].mperf = mperf;
		perf_data[cpu].tsc = tsc;
		return 0.0;
	}

	delta_mperf = mperf - perf_data[cpu].mperf;
	delta_tsc = tsc - perf_data[cpu].tsc;

	perf_data[cpu].mperf = mperf;
	perf_data[cpu].tsc = tsc;

	result = 100.0 *(float)delta_mperf / (float)delta_tsc;

	//	current_moving_average = moving_average(perf_samples, cpu, result);
	//	perf_samples[cpu].last_moving_average = current_moving_average;
	//	thd_log_debug("DTS sensor %d c0 mov average %u\n", cpu, current_moving_average);

	return result;
}

bool cthd_topology::check_load_for_calibration(unsigned char cpu_mask)
{
	float result;

	// First pass just to store the last value
	for(int i = 0; i < no_cpu; ++i)
	{
		get_c0_present_value(i);
	}
	// First pass just to store the last value
	sleep(1);
	for(int i = 0; i < no_cpu; ++i)
	{
		// Skip CPU if not requested
		if(cpu_mask &(1 << i))
		{
			result = get_c0_present_value(i);
			// if load is more than 5% we shouldn't continue
			// as it will affect results and retry later
			//thd_log_info ("cpu %d C0  %6.2f\n",i, result);
			if(result > 20.0)
			{
				thd_log_info("CPU %d C0  %6.2f\n", i, result);
				return false;
			}
		}
	}

	return true;
}

int cthd_topology::calibrate()
{
	int ret;
	int i, j;

	// Reset moving average
	memset(temp_data, 0, sizeof(*temp_data) * no_sensors);

	for(i = 0; i < no_cpu; ++i)
	{
		cpu_sensor_rel[i].sensor_id =  - 1;
	}

	for(i = 0; i < no_sensors; ++i)
	{
		sensor_cpu_rel[i].cpu_id =  - 1;
	}

	for(i = 0; i < no_cpu; ++i)
	{
		int max = 0;
		int max_index = 0;
		int diff;

		if(check_load_for_calibration(0xff) == false)
		{
			// Not a right time to do calibration
			// retry at later time
			thd_log_warn("Calibration process aborted because of unsuitable CPU load \n")
	;
			return  - 1;
		}

		designated_cpu = i;
		memset(temp_data, 0, sizeof(*temp_data) *no_sensors);
		pthread_attr_init(&thd_attr);
		pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
		terminate_thread = false;
		ret = pthread_create(&load_thread, &thd_attr, load_thread_loop, (void*)this);
		if (ret < 0)
			return ret;
		ret = pthread_create(&temp_thread, &thd_attr, temp_thread_loop, (void*)this);
		if (ret < 0)
			return ret;
		thd_log_info("Started %d seconds sleep on main thread \n", measurment_time);
		sleep(measurment_time);
		thd_log_info("End %d seconds sleep on main thread \n", measurment_time);
		terminate_thread = true;
		sleep(1);
		pthread_cancel(load_thread);
		pthread_cancel(temp_thread);

		if(cal_failed)
		{
			thd_log_warn("Calibration process aborted because of unsuitable CPU load \n")
	;
			return  - 1;
		}

		for(int j = 0; j < no_sensors; ++j)
		{
			diff = temp_data[j].last_moving_average;
			if(diff == 0)
				continue;
			if(max < diff)
			{
				max = diff;
				max_index = j;
			}
			cpu_sensor_rel[i].sensor_id = max_index;
			thd_log_info("cpu %d sensor %d current %d max %d:%d\n", i, j, diff,
	max_index, max);
		}
	}

	// Sanitize
	bool valid = true;
	for(i = 0; i < no_cpu; ++i)
	{
		int count = 0;
		unsigned int mask = 0;
		for(j = 0; j < no_sensors; ++j)
		{
			if(i != j && cpu_sensor_rel[j].sensor_id !=  - 1 &&
	cpu_sensor_rel[i].sensor_id == cpu_sensor_rel[j].sensor_id)
			{
				count++;
				mask |= ((1 << i) | (1 << j));
				sensor_cpu_rel[cpu_sensor_rel[i].sensor_id].cpu_id = mask;
				if(count > 2)
				{
					valid = false;
					break;
				}

			}
		}
	}

	// Check if every CPU is covered
	if(valid)
	{
		for(i = 0; i < no_cpu; ++i)
		{
			if(cpu_sensor_rel[i].sensor_id ==  - 1)
			{
				valid = false;
				break;
			}
		}
	}
	if(valid)
	{
		for(i = 0; i < no_cpu; ++i)
		{
			//if (cpu_sensor_rel[i].sensor_id != -1)
			thd_log_info("cpu_sensor_rel cpu%d sensor %d\n", i,
	cpu_sensor_rel[i].sensor_id);
		}

		for(i = 0; i < no_sensors; ++i)
		{
			if(sensor_cpu_rel[i].cpu_id !=  - 1)
			{
				thd_log_info("sensor_cpu rel sensor %d cpu %x\n", i,
	sensor_cpu_rel[i].cpu_id);
				store_configuration(i, sensor_cpu_rel[i].cpu_id);
			}
		}
	}
	else
	{
		thd_log_info("Can't reliably tie sensor and CPU\n");
	}

	configuration_saved();
	return 0; 
	// calibration process is done, it is possible that we can't do per cpu control
}

void cthd_topology::store_configuration(int index, unsigned int mask)
{
	std::stringstream filename;
	std::ofstream file;

	filename << TDCONFDIR << "/" << "dts_" << index << "_sensor_mask.conf";
	std::ofstream fout(filename.str().c_str());
	if(fout.good())
	{
		fout << mask;
	}
	thd_log_info("storing sensor-cpu mask %x\n", mask);
	fout.close();
}

void cthd_topology::configuration_saved()
{
	std::stringstream filename;
	std::ofstream file;

	filename << TDCONFDIR << "/" << "dts_cal_completed";
	std::ofstream fout(filename.str().c_str());
	if(fout.good())
	{
		fout << 1;
	}
	fout.close();
}

bool cthd_topology::check_config_saved()
{
	std::stringstream filename;
	bool done = false;

	filename << TDCONFDIR << "/" << "dts_cal_completed";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if(ifs.good())
	{
		done = true;
	}
	ifs.close();

	return done;
}

