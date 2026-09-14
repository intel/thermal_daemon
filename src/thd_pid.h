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
#include <string>
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
typedef enum : std::uint8_t {
	PID_ABSOLUTE,
	PID_INCREMENTAL
} pid_mode_t;

typedef struct
{
	int valid;
	double kp;
	double ki;
	double kd;
	pid_mode_t mode;
	/*
	 * When set, the configured Kp/Ki/Kd are trimmed at runtime by
	 * cthd_pid_adaptive.  Opt-in via <PidAdaptive>1</PidAdaptive>.
	 */
	bool adaptive;
}pid_param_t;

/*
 * Adaptive gain trimming.
 *
 * Rather than adapting Kp/Ki/Kd directly, this adapts a *multiplicative trim*
 * on each of the three configured gains:
 *
 *   Kp_eff = kp * trim[0],  Ki_eff = ki * trim[1],  Kd_eff = kd * trim[2]
 *
 *   trim[i] = R ^ tanh(s[i]),  s[i] = sum_j coeff[i][j] * X[j]
 *
 * where X is the (normalised) error, error integral and error derivative, and
 * coeff is adjusted by gradient descent on the tracking error each poll.
 *
 * Trimming rather than replacing is what makes this safe to run in a thermal
 * daemon:
 *
 *   - trim[i] is bounded to [1/R, R], so an adapted gain can never be more
 *     than R times off the value that was tuned by hand.
 *   - trim[i] > 0 always, so adaptation can never flip the sign of a gain
 *     and turn the loop into positive feedback.
 *   - trim(s = 0) == 1 exactly, and coeff starts at zero.  A fresh controller
 *     therefore reproduces the fixed-gain behaviour bit-for-bit on the first
 *     poll, and only ever deviates from a known-good baseline.
 *   - a slew limit caps how fast trim may move between polls.
 *
 * Inputs are normalised before use so that the step size does not depend on
 * whether the error is expressed in millidegrees or the integral has been
 * accumulating for an hour.
 *
 * The adaptation direction needs the sign of de/du, and that is taken from the
 * configuration rather than measured: for a plant with dT/du = g, stability of
 * u = kp*e + ... requires kp*g < 0, so sign(de/du) is -sign(kp).
 *
 * It is tempting to verify that at runtime by correlating our own output
 * changes against the error changes that follow, but in closed loop that
 * measurement is invalid: u is computed *from* e, so du and de are correlated
 * through the controller (du/de = kp) and not through the plant.  Such an
 * estimator converges to sign(kp), which is the wrong sign whenever kp > 0,
 * and the resulting inverted gradient walks every gain to its bound.  A
 * configured loop that had the sign wrong would already be diverging with
 * fixed gains, so there is nothing here worth measuring.
 */
#define PID_ADAPT_DIM		3

class cthd_pid_adaptive {

private:
	double coeff[PID_ADAPT_DIM][PID_ADAPT_DIM];
	double prev_dcoeff[PID_ADAPT_DIM][PID_ADAPT_DIM];
	/* Slew limited trim actually handed to the controller. */
	double prev_trim[PID_ADAPT_DIM];
	unsigned int updates_since_save;
	std::string key;

	bool store() const;

public:
	cthd_pid_adaptive() { reset(); }

	void reset();

	/* Persistence is keyed on a caller supplied, filename safe name. */
	void set_key(const std::string &_key);
	bool is_persistent() const { return !key.empty(); }

	/*
	 * trim[] is the multiplier for each gain; tanh_sum[] is the intermediate
	 * tanh(s[i]), kept so adapt() need not recompute it.  Not const: the
	 * returned trim is slew limited against the previous call, so calling
	 * this advances state.
	 */
	void compute_trim(const double X[PID_ADAPT_DIM], double trim[PID_ADAPT_DIM],
			double tanh_sum[PID_ADAPT_DIM]);

	/*
	 * gsign[] carries sign(kp), sign(ki), sign(kd); the plant sign is derived
	 * from gsign[0], see the class comment.
	 */
	void adapt(const double X[PID_ADAPT_DIM], const double trim[PID_ADAPT_DIM],
			const double tanh_sum[PID_ADAPT_DIM], const double gsign[PID_ADAPT_DIM],
			double err_n);

	/*
	 * Write the coefficients out now instead of waiting for the next periodic
	 * save.  adapt() only persists every PID_ADAPT_SAVE_INTERVAL updates, so
	 * without this a run that ends before the interval elapses -- and every
	 * run shorter than that, since the counter restarts at zero -- would
	 * learn and then throw the result away.  No-op when not persistent.
	 */
	void flush();
};

class cthd_pid {

private:
	double err_sum, last_err;
	time_t last_time;
	unsigned int target_temp;
	pid_mode_t mode;
	bool adaptive;
	cthd_pid_adaptive trim_ctrl;

	/* Applies the current trim to kp/ki/kd, then advances the adaptation. */
	double adaptive_output(double error, double _err_sum, double d_err);

public:
	double kp, ki, kd;
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
	pid_mode_t get_pid_mode() const { return mode; }

	/*
	 * Enable adaptive gain trimming.  "_key" names the file under TDRUNDIR
	 * used to carry adapted coefficients across a daemon restart; pass an
	 * empty string to keep them in memory only.
	 */
	void set_pid_adaptive(bool enable, const std::string &_key = "");
	bool is_pid_adaptive() const { return adaptive; }

	/* Persist adapted coefficients now.  See cthd_pid_adaptive::flush(). */
	void pid_adaptive_flush() {
		if (adaptive)
			trim_ctrl.flush();
	}

	int pid_output(unsigned int curr_temp, int initial_value = 0);
	void set_target_temp(unsigned int temp) {
		target_temp = temp;
	}
	void reset() {
		err_sum = last_err = last_time = 0;
	}
};
