/*
 * thd_pid.h: PID controller
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

#ifndef THD_PID_H
#define THD_PID_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include <vector>

#define DWATT_HIGH (.95)
#define DWATT_LOW  (.01)

typedef struct {
	double kp;  /* Controller gain from Dialog Box */
	double ki; /* Time-constant for I action from Dialog Box */
	double kd; /* Time-constant for D action from Dialog Box */
	double ts;
	double k_lpf;
	double k0;
	double k1;
	double k2;
	double k3;
	double lpf1;
	double lpf2;
	/* intermediate data */
	double pp;
	double pi;
	double pd;

	double t_target;
	double y_k;
}pid_params_t;

class cthd_pid {

private:
	csys_fs 		cdev_sysfs;
	int		 		last_time;
	float 			err_sum, last_err;
	float 			kp, ki, kd;
	std::vector <int> 	cdev_indexes;

	int				cal_set_pt_active;
	int				last_temp;
	int				stable_cnt;
	int				cal_high_temp;
	int				cal_low_temp;


	pthread_attr_t crazy_thread_attr;
	pthread_t 	crazy_running_thread;
	bool		calibrated;
	std::string	sensor_path;

	int 		getMilliCount();
	unsigned int read_zone_temp();
	void	inc_cdevs_state();
	void	dec_cdevs_state();
	void	cdev_set_state(int state);
	int  activate_cdev(int temp, int set_point);

public:
	static const int calibration_set_point = 75000;
	static const float calibration_kdev_start = 0.5;
	static const float calibration_kdev_inc_unit = 0.5;

	cthd_pid();

	void set_pid_params(float Kp, float Ki, float Kd);
	void calibrate();
	int update_pid(int temp, int setpoint);
	void 		close_loop_tune();

	void add_sensor_path(std::string path);
	void add_cdev_index(int index);

	void print_pid_constants();

	//
		double xk_1;	// Previous value of xk
		double lpf, lpf_1, lpf_2;
		static const int Ts = 5;
		pid_params_t p_param;
		void init_pid_controller();
		double get_pid_output(double set_point, double xk, int state);
		void open_loop_tune();
	//

};

#endif
