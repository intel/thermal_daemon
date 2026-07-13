/*
 * thd_pid.cpp: pid implementation
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
#include "thd_pid.h"

cthd_pid::cthd_pid() {
	kp = 0.0005;
	ki = kd = 0.0001;
	last_time = 0;
	err_sum = 0.0;
	last_err = 0.0;
	target_temp = 0;
	mode = PID_ABSOLUTE;
}

int cthd_pid::pid_output(unsigned int curr_temp, int initial_value) {
	double output;
	/* Use signed arithmetic to avoid unsigned wrap-around when
	 * curr_temp < target_temp */
	int error = (int)curr_temp - (int)target_temp;

	time_t now;
	time(&now);

	if (last_time == 0) {
		/* First call: initialise state. */
		last_time = now;
		last_err = error;

		if (mode == PID_INCREMENTAL) {
			/* No integral history yet — return Kp*e so the first
			 * poll already applies a proportional correction. */
			err_sum = 0;
			output = kp * error;
		} else {
			/* Absolute mode: seed err_sum for bumpless start so the
			 * first output equals initial_value. d_err assumed zero. */
			err_sum = ki ? (initial_value - kp * error) / ki : 0;
			output = kp * error + ki * err_sum;
		}
		thd_log_debug("pid first call mode:%s e:%d out:%d\n",
				mode == PID_INCREMENTAL ? "inc" : "abs",
				error, (int)output);
		return (int)output;
	}

	time_t timeChange = (now - last_time);

	/*
	 * Both modes use the same PID formula:
	 *   u = Kp*e + Ki*∫e*dt + Kd*de/dt
	 *
	 * The difference is in the caller (thd_cdev_set_state):
	 *   Absolute:    new_state = min_state  ± u  (fixed steady-state)
	 *   Incremental: new_state = curr_state ± u  (keeps reducing each poll)
	 */
	err_sum += (double)error * timeChange;

	double d_err = timeChange ?
			(double)(error - last_err) / timeChange : 0.0;

	output = kp * error + ki * err_sum + kd * d_err;

	thd_log_debug("pid_%s e:%d kp:%g ki_sum:%g kd:%g out:%d\n",
			mode == PID_INCREMENTAL ? "inc" : "abs",
			error, kp * error, ki * err_sum, kd * d_err, (int)output);

	last_err = error;
	last_time = now;

	thd_log_debug("pid_output curr:%u tgt:%u mode:%d out:%d\n",
			curr_temp, target_temp, (int)mode, (int)output);
	return (int)output;
}
