/*
 * thd_pid.h: pid interface
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "thermald.h"
#include <cstdint>
#include <time.h>

/*
 * PID controller mode:
 *
 *   PID_ABSOLUTE    - Output is the absolute desired state.
 *                     u = Kp*e + Ki*∫e*dt + Kd*de/dt
 *                     Caller: new_state = min_state ± u
 *                     Finds a fixed steady-state proportional to the error.
 *
 *   PID_INCREMENTAL - Same formula as absolute, but output is applied as a
 *                     delta to the current state instead of to min_state.
 *                     Caller: new_state = curr_state ± u
 *                     Power limit keeps decreasing each poll while
 *                     temperature stays above the trip threshold.
 */
/* Longest time step (seconds) one call of a time scaled PID acts on */
#define PID_TIME_SCALED_MAX_DT	10

typedef enum : std::uint8_t {
	PID_ABSOLUTE,
	PID_INCREMENTAL
} pid_mode_t;

typedef struct pid_param
{
	int valid;
	double kp;
	double ki;
	double kd;
	pid_mode_t mode;
	/*
	 * Incremental mode only: below the trip keep running the PID, so the
	 * limit is released in proportion to the (negative) error instead of
	 * being restored at once.
	 */
	int gradual_release = 0;
	/* Control towards trip temperature - set_point_offset (millicelsius) */
	int set_point_offset = 0;
	/*
	 * Incremental mode only: output Kp*e*dt + Kd*(e - last e), so the
	 * gains are per second and don't depend on how often it is called.
	 */
	int time_scaled = 0;
}pid_param_t;

class cthd_pid {

private:
	double err_sum, last_err;
	time_t last_time;
	unsigned int target_temp;
	pid_mode_t mode;
	bool time_scaled;

public:
	double kp, ki, kd;
	/* Consecutive backstop calls, see cthd_cdev::pid_gradual_bound() */
	int boost_pow;
	cthd_pid();
	cthd_pid(const cthd_pid& x) = default;
	
	~cthd_pid() { }

	cthd_pid& operator=(const cthd_pid& x) = default;
	
	void set_pid_param(double _kp, double _ki, double _kd)
	{
		kp = _kp;
		ki = _ki;
		kd = _kd;
	}
	void set_pid_mode(pid_mode_t m) { mode = m; }
	void set_time_scaled(bool t) { time_scaled = t; }
	pid_mode_t get_pid_mode() const { return mode; }

	int pid_output(unsigned int curr_temp, int initial_value = 0);
	void set_target_temp(unsigned int temp) {
		target_temp = temp;
	}
	void reset() {
		err_sum = last_err = last_time = 0;
		boost_pow = 0;
	}
};
