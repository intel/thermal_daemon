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
#include "thd_engine.h"
#include "thd_cdev.h"
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

void cthd_pid::add_sensor_path(std::string path)
{
	sensor_path = path;
	thd_log_debug("add_sensor_path %s\n", sensor_path.c_str());

}

int cthd_pid::update_pid(int input, int setpoint)
{
#if 0
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
#else
	double output;
	output = get_pid_output((double)setpoint/1000, (double)input/1000, 1);
#endif
}

unsigned int cthd_pid::read_zone_temp()
{
	csys_fs sysfs;
	std::string buffer;
	int temperature;

	if (!sysfs.exists(sensor_path)) {
		thd_log_warn("read_zone_temp: No temp sysfs for reading temp: %s\n", sensor_path.c_str());
		return 0;
	}
	sysfs.read(sensor_path, buffer);
	std::istringstream(buffer) >> temperature;

	return temperature;
}

void cthd_pid::inc_cdevs_state()
{
	int state;

	for (int i=0; i<cdev_indexes.size(); ++i) {
		cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
		state = cdev->get_curr_state();
		if ((state + 1) <= cdev->get_max_state()) {
			cdev->set_curr_state(state+1, 0);
		}
	}
}

void cthd_pid::dec_cdevs_state()
{
	int state;

	for (int i=0; i<cdev_indexes.size(); ++i) {
		cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
		state = cdev->get_curr_state();
		if ((state - 1) >= 0) {
			cdev->set_curr_state(state-1, 0);
		}
	}
}

void cthd_pid::cdev_set_state(int state)
{
	for (int i=0; i<cdev_indexes.size(); ++i) {
		cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
		if (state >= 0 && state <= cdev->get_max_state()) {
			cdev->set_curr_state(state, 0);
		}
	}
}

int cthd_pid::activate_cdev(int temp, int set_point)
{
	int state;

	state = update_pid(temp, set_point);
	cdev_set_state(abs(state));
	kp += calibration_kdev_inc_unit;

	return 0;
}

void cthd_pid::close_loop_tune()
{
	int tid;
	int temperature;
	int state;

	tid = omp_get_thread_num();
	// only master thread does this
	if (tid == 0) {
		int now = getMilliCount();
		if (last_time == 0)
			last_time = now;
		if ((now - last_time) < 2000) {
			return;
		} else {
			temperature = read_zone_temp();
			if (!cal_set_pt_active && temperature > calibration_set_point) {
				thd_log_debug("Calibration Set point reached %d\n", temperature);
				cal_set_pt_active = 1;
				cal_high_temp = cal_low_temp = temperature;
				stable_cnt = 0;
				activate_cdev(temperature, calibration_set_point);
			}
			if (cal_set_pt_active) {
				if (temperature >= cal_high_temp)
					cal_high_temp = temperature;
				if (temperature < cal_high_temp)
					cal_low_temp = temperature;
				if ( (cal_high_temp - temperature) == (temperature - cal_low_temp) ) {
					stable_cnt++;
					thd_log_debug("oscilation constant %d \n", stable_cnt);
				} else if (last_temp == temperature){
					stable_cnt++;
					thd_log_debug("Same %d \n", stable_cnt);
				}
				else if (temperature <= calibration_set_point){
									stable_cnt++;
									thd_log_debug("Same %d \n", stable_cnt);
				}
				else {
					activate_cdev(temperature, calibration_set_point);
					stable_cnt--;
				}
				if (stable_cnt > 5)
					thread_terminate = 1;
			}
			last_temp = temperature;
			last_time = now;
			// do processing
			thd_log_info("close loop tune %d, %d, %d\n", temperature, cal_high_temp, cal_low_temp);
		}
	}
}

static void *crazy_thread(void *arg)
{
    int no_cores;
    cthd_pid *ptr_obj = (cthd_pid*) arg;

    omp_set_dynamic(0);
    no_cores = omp_get_num_procs();
    omp_set_num_threads(no_cores/2);
    #pragma omp parallel
    for(;;) {
#if 0
    	ptr_obj->close_loop_tune();
#else
    	ptr_obj->open_loop_tune();
#endif
    	if (thread_terminate)
    		break;
     }
    pthread_exit(NULL);
}

void cthd_pid::calibrate()
{
	int ret;
	void *status;

	if (cdev_indexes.size() < 0) {
		thd_log_warn("There are no cooling devices associated, so can't calibrate");
		return;
	} else
	{
		for (int i=0; i<cdev_indexes.size(); ++i) {
			thd_log_debug("cdev_index %d\n", cdev_indexes[i]);
			cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
			thd_log_debug("cdev at index %d\n", cdev->thd_cdev_get_index());

		}
	}
	cal_set_pt_active = 0;
	cal_high_temp = cal_low_temp = 0;
	kp = calibration_kdev_start;
	ki = kd = 0;
	thread_terminate = 0;

	for (int i=0; i<cdev_indexes.size(); ++i) {
		cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
			cdev->set_curr_state(0, 0);
	}

	pthread_attr_init(&crazy_thread_attr);
	pthread_attr_setdetachstate(&crazy_thread_attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(&crazy_running_thread, &crazy_thread_attr, crazy_thread, (void *)this);
	if (ret < 0) {
		thd_log_warn("crazy thread creation failed \n");
		return;
	}
	sleep(120);
	thread_terminate = 1;

	ret = pthread_join(crazy_running_thread, &status);
	thd_log_debug("calibrate thread done");
}

double cthd_pid::get_pid_output(double set_point, double xk, int state)
{
	double ek;
	double lpf;
	double yk;

	p_param.t_target = set_point;
	ek = set_point - xk; /* error */
	if (ek >= 0.0) {
		return 0;
	}

	yk = 0.0;
	lpf = p_param.lpf1 * lpf_1 + p_param.lpf2 * (xk + xk_1);
	if (state & 0x1) {
		p_param.pp = -p_param.kp * (xk - xk_1);
		p_param.pi = p_param.kp * (p_param.ts/p_param.ki) * ek;
		p_param.pd = -(p_param.kp * p_param.kd / p_param.ts) *
			(lpf - 2.0 * lpf_1 + lpf_2);
		yk += p_param.pp + p_param.pi + p_param.pd;

		if (yk > 0)
			yk = p_param.pp = p_param.pi = p_param.pd = 0.0;
	} else
		yk = p_param.pp = p_param.pi = p_param.pd = 0.0;

	xk_1 = xk;

	lpf_2 = lpf_1; /* update LPF */
	lpf_1 = lpf;

#if 0
	/* clamp output adjustment range */
	if (yk < -DWATT_HIGH)
		yk = -DWATT_HIGH;
	else if (yk > -DWATT_LOW)
		yk = -DWATT_LOW;
#endif
	p_param.y_k = yk;

	thd_log_debug("target temp%3.3f x: %3.3f  y:%3.3f\n", p_param.t_target, xk, yk);
	return yk;
}

void cthd_pid::init_pid_controller()
{
	p_param.ts = 5;

	p_param.kp = 0.26;
	p_param.ki = 5.0;
	p_param.kd = 0.19;

	p_param.k_lpf = .1 * p_param.kd;
	p_param.k1 = p_param.kp * p_param.kd / p_param.ts;
	p_param.lpf1 = (2.0 * p_param.k_lpf - p_param.ts) /
		(2.0 * p_param.k_lpf + p_param.ts);
	p_param.lpf2 = p_param.ts / (2.0 * p_param.k_lpf + p_param.ts);


}

void cthd_pid::open_loop_tune()
{
	int tid;
	int temperature;
	int state;

	tid = omp_get_thread_num();
	// only master thread does this
	if (tid == 0) {
		int now = getMilliCount();
		int start = 0;
		if (last_time == 0)
			last_time = now;
		//thd_log_debug("TiD = 0 now%d last%d\n", now, last_time);
		if (cal_set_pt_active == 0 && (now - last_time) > 1000) {
			temperature = read_zone_temp();
			thd_log_debug("Temp %d \n", temperature);
			if (temperature > calibration_set_point) {
				thd_log_debug("Set point reached \n");
				cal_set_pt_active = 1;
				// Set cdev state to 20%
				for (int i=0; i<cdev_indexes.size(); ++i) {
					cthd_cdev *cdev = thd_engine->thd_get_cdev_at_index(cdev_indexes[i]);
					state = cdev->get_max_state() * 10/100;
					if (state > 0) {
						cdev->set_curr_state(state, 0);
					}
				}
				start = now;
			}
			last_time = now;
		} else	if (cal_set_pt_active && (now - last_time) > 1000) {
			temperature = read_zone_temp();
			thd_log_debug("Temp %d \n", temperature);
			if (temperature > (calibration_set_point + (calibration_set_point*10/100))) {
				thd_log_debug("Dead time is %d\n", now - start);
				//thread_terminate = 1;
			}
			last_time = now;

		}

		last_temp = temperature;

	}
}
