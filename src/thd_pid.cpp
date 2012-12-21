/*
 * thd_pid.cpp: pid implementation
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
#include "thd_pid.h"
#include <pthread.h>
#include <omp.h>
#include <sys/timeb.h>

static int thread_terminate;

cthd_pid::cthd_pid()
	: kp(1.0),
	  ki(0),
	  kd(0),
	  calibrated(0),
	  last_time(0)
{

}

#if 0
cthd_pid::cthd_pid(float Kp, float Ki, float Kd)
	: kp(Kp),
	  ki(Ki),
	  kd(Kd),
	  calibrated(0),
	  last_time(0)
{

}
#endif

void cthd_pid::set_pid_params(float Kp, float Ki, float Kd)
{
	kp = Kp;
	ki = Ki;
	kd = Kd;
	thd_log_debug("set_pid_params kp %f ki %f kd %f\n", kp, ki, kd);
}

void cthd_pid::add_cdev_index(int _index)
{
	cdev_indexes.push_back(_index);
}

int cthd_pid::getMilliCount()
{
	timeb tb;
	ftime(&tb);
	int nCount = tb.millitm + (tb.time & 0xfffff) * 1000;
	return nCount;
}

void cthd_pid::print_pid_constants()
{
	thd_log_debug("print_pid_params this*%x kp %f ki %f kd %f\n", this, kp, ki, kd);
}

int cthd_pid::update_pid(int input, int setpoint)
{
	float output;

	int now = getMilliCount();
	if (last_time == 0)
		last_time = now;
	float timeChange = (double)(now - last_time);

	float error = (setpoint - input)/1000; // convert from milli
	err_sum += (error * timeChange);
	float d_err = (error - last_err) / timeChange;

	thd_log_debug("kp %f ki %f kd %f\n", kp, ki, kd);

	/*Compute PID Output*/
	output = kp * error + ki * err_sum + kd * d_err;

	thd_log_debug("update_pid %d %d %f %f\n", now, last_time, error, output);

	/*Remember some variables for next time*/
	last_err = error;
	last_time = now;

	return (int)output;
}

void cthd_pid::close_loop_tune()
{
	int tid;

	tid = omp_get_thread_num();
	// only master thread does this
	if (tid == 0) {
		int now = getMilliCount();
		if (last_time == 0)
			last_time = now;
		if ((now - last_time) < 1000) {
			return;
		} else {
			last_time = now;
			// do processing
			thd_log_debug("close loop tune\n");
		}
	}
}

static void *crazy_thread(void *arg)
{
    int no_cores;
    cthd_pid *ptr_obj = (cthd_pid*) arg;

    omp_set_dynamic(0);
    no_cores = omp_get_num_procs();
    omp_set_num_threads(no_cores);
    #pragma omp parallel
    for(;;) {
    	ptr_obj->close_loop_tune();
    	if (thread_terminate)
    		break;
     }
    pthread_exit(NULL);
}

void cthd_pid::calibrate()
{
	int ret;
	void *status;

	thread_terminate = 0;
	pthread_attr_init(&crazy_thread_attr);
	pthread_attr_setdetachstate(&crazy_thread_attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(&crazy_running_thread, &crazy_thread_attr, crazy_thread, (void *)this);
	if (ret < 0) {
		thd_log_warn("crazy thread creation failed \n");
		return;
	}
	sleep(10);
	thread_terminate = 1;

	ret = pthread_join(crazy_running_thread, &status);
	thd_log_debug("calibrate thread done");
}
