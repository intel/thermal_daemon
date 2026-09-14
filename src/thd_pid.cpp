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
#include "thd_util.h"

#include <climits>
#include <cmath>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <unistd.h>

/*
 * Tunables for the adaptive trim layer.  These are deliberately conservative:
 * the point of the layer is to shave the last bit of overshoot off a
 * hand tuned loop, not to tune one from scratch.
 */
#define PID_ADAPT_TRIM_RANGE	4.0	/* trim bounded to [1/4, 4] */
/*
 * The adaptation loop must be much slower than the control loop, or the two
 * fight each other: a zone with a 60s time constant polled every 4s needs
 * ~15 polls just to show the effect of a gain change, so a trim that moves
 * appreciably per poll is reacting to its own past output.  Hence a small
 * step size *and* a hard slew limit on how fast the trim may move.
 */
#ifndef PID_ADAPT_LR
#define PID_ADAPT_LR		0.001
#endif
#ifndef PID_ADAPT_MAX_STEP
#define PID_ADAPT_MAX_STEP		0.002	/* max change in trim per poll */
#endif
#define PID_ADAPT_MOMENTUM		0.5
#define PID_ADAPT_COEFF_MAX		6.0	/* tanh() is saturated well before this */

/*
 * Input normalisation references.  Temperature is in millidegrees, so
 * ERR_REF is 2C of error, INT_REF is 2C sustained for 10s and DERIV_REF is
 * 0.5C per second.
 */
#define PID_ADAPT_ERR_REF		2000.0
#define PID_ADAPT_INT_REF		20000.0
#define PID_ADAPT_DERIV_REF	500.0

/* Below this normalised error the loop is at target; do not adapt on noise. */
#define PID_ADAPT_DEADBAND		0.02
/*
 * Lead term on the error derivative in the adaptation objective.
 *
 * Minimising the *instantaneous* error is the wrong objective for a thermal
 * plant: the plant lags the actuator by tens of seconds, so "reduce the error
 * I can see right now" always argues for more gain, and the oscillation that
 * buys only shows up several polls later.  Left like that the trim walks
 * straight to its upper bound and makes overshoot worse, not better.
 *
 * So the cost is built on a lead-compensated error
 *
 *   e_eff = e_n + PID_ADAPT_LEAD * d_n
 *
 * which already counts the error the plant is heading towards.  When the zone
 * is hot but cooling quickly, e_eff is small (or negative) and the update
 * backs the gains off instead of piling more on.
 */
#ifndef PID_ADAPT_LEAD
#define PID_ADAPT_LEAD		4.0
#endif
/* Persist at most once every N updates. */
#define PID_ADAPT_SAVE_INTERVAL	60
/* v2 dropped the persisted de/du sign estimate; see thd_pid.h. */
#define PID_ADAPT_FILE_VERSION	2

static inline double pid_adapt_clamp(double v, double lo, double hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

/*
 * Squash into (-1, 1) so gradients stay O(lr).
 *
 * tanh() rather than a hard clamp on purpose.  The integral term in
 * particular has no natural bound -- err_sum accumulates error * dt, so a few
 * degrees of error for a few polls already exceeds any fixed reference.  A
 * hard clamp pins that input at exactly +-1 for long stretches, and a pinned
 * input makes the gradient a constant, which walks the coefficients straight
 * into the trim bound.  tanh() keeps the input strictly inside the range and
 * lets the gradient shrink as the input grows.
 */
static inline double pid_adapt_norm(double v, double ref) {
	return tanh(v / ref);
}

void cthd_pid_adaptive::reset() {
	for (int i = 0; i < PID_ADAPT_DIM; ++i) {
		for (int j = 0; j < PID_ADAPT_DIM; ++j) {
			coeff[i][j] = 0.0;
			prev_dcoeff[i][j] = 0.0;
		}
		prev_trim[i] = 1.0;
	}
	updates_since_save = 0;
}

void cthd_pid_adaptive::compute_trim(const double X[PID_ADAPT_DIM],
		double trim[PID_ADAPT_DIM], double tanh_sum[PID_ADAPT_DIM]) {
	for (int i = 0; i < PID_ADAPT_DIM; ++i) {
		double sum = 0.0;

		for (int j = 0; j < PID_ADAPT_DIM; ++j)
			sum += coeff[i][j] * X[j];

		tanh_sum[i] = tanh(sum);
		/* R^tanh(s): equals 1 at s == 0, bounded to [1/R, R]. */
		double raw = exp(tanh_sum[i] * log(PID_ADAPT_TRIM_RANGE));

		/*
		 * Slew limit.  The computed trim can jump when the inputs jump
		 * (a load step moves all three at once); the gains must not.
		 */
		trim[i] = prev_trim[i]
				+ pid_adapt_clamp(raw - prev_trim[i], -PID_ADAPT_MAX_STEP,
						PID_ADAPT_MAX_STEP);
		prev_trim[i] = trim[i];
	}
}

void cthd_pid_adaptive::adapt(const double X[PID_ADAPT_DIM],
		const double trim[PID_ADAPT_DIM], const double tanh_sum[PID_ADAPT_DIM],
		const double gsign[PID_ADAPT_DIM], double err_n) {
	/* At target: hold the coefficients rather than chase sensor noise. */
	if (fabs(err_n) < PID_ADAPT_DEADBAND)
		return;

	/*
	 * Lead-compensated error: what the error is heading towards, not just
	 * what it is.  X[2] is the normalised de/dt.  See PID_ADAPT_LEAD.
	 */
	double err_eff = pid_adapt_clamp(err_n + PID_ADAPT_LEAD * X[2], -1.0, 1.0);

	/*
	 * sign(de/du) = -sign(kp), taken from the configuration.  gsign[0] is
	 * sign(kp), so the plant sign is just its negation.  See thd_pid.h for
	 * why this is not measured at runtime.
	 */
	double plant_sign = -gsign[0];
	double log_r = log(PID_ADAPT_TRIM_RANGE);

	for (int i = 0; i < PID_ADAPT_DIM; ++i) {
		/*
		 * dE/ds[i] for E = 0.5 * err_eff^2, with
		 *   de/du          = plant_sign
		 *   du/dtrim[i]   ~ sign(gain[i]) * X[i]
		 *   dtrim[i]/ds[i] = trim[i] * log(R) * sech^2(s[i])
		 */
		double dtrim_ds = trim[i] * log_r
				* (1.0 - tanh_sum[i] * tanh_sum[i]);
		double dE_ds = err_eff * plant_sign * gsign[i] * X[i] * dtrim_ds;

		for (int j = 0; j < PID_ADAPT_DIM; ++j) {
			/* Gradient descent on E, with momentum. */
			double dcoeff = -PID_ADAPT_LR * dE_ds * X[j]
					+ PID_ADAPT_MOMENTUM * prev_dcoeff[i][j];

			if (!std::isfinite(dcoeff))
				continue;

			coeff[i][j] = pid_adapt_clamp(coeff[i][j] + dcoeff, -PID_ADAPT_COEFF_MAX,
					PID_ADAPT_COEFF_MAX);
			prev_dcoeff[i][j] = dcoeff;
		}
	}

	if (is_persistent() && ++updates_since_save >= PID_ADAPT_SAVE_INTERVAL) {
		updates_since_save = 0;
		store();
	}
}

static std::string pid_adapt_file_name(const std::string &key) {
	std::ostringstream filename;

	filename << TDRUNDIR << "/" << "thd_pid_adapt." << key << ".conf";

	return filename.str();
}

void cthd_pid_adaptive::set_key(const std::string &_key) {
	key.clear();

	if (_key.empty())
		return;

	/* Refuse anything that could escape TDRUNDIR. */
	if (!is_valid_thermal_object_name(_key)) {
		thd_log_warn("pid_adapt: invalid persistence key '%s'\n", _key.c_str());
		return;
	}
	/* Spaces are legal in cdev names but awkward in file names. */
	if (_key.find(' ') != std::string::npos) {
		thd_log_warn("pid_adapt: persistence key '%s' has spaces, not saving\n",
				_key.c_str());
		return;
	}

	key = _key;

	/*
	 * Restore previously adapted coefficients.  Anything unparsable or out
	 * of range leaves the zeroed coefficients in place, i.e. plain fixed
	 * gains.
	 */
	std::string name = pid_adapt_file_name(key);
	int fd = ::open(name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0)
		return;

	char buf[512];
	ssize_t len = ::read(fd, buf, sizeof(buf) - 1);
	::close(fd);
	if (len <= 0)
		return;
	buf[len] = '\0';

	std::istringstream is(buf);
	int version = 0;

	if (!(is >> version) || version != PID_ADAPT_FILE_VERSION) {
		thd_log_warn("pid_adapt: %s version mismatch, ignoring\n", name.c_str());
		return;
	}

	double _coeff[PID_ADAPT_DIM][PID_ADAPT_DIM];

	for (auto &row : _coeff) {
		for (double &w : row) {
			if (!(is >> w)
					|| !is_valid_finite_value(w, -PID_ADAPT_COEFF_MAX, PID_ADAPT_COEFF_MAX))
				goto bad_file;
		}
	}

	for (int i = 0; i < PID_ADAPT_DIM; ++i)
		for (int j = 0; j < PID_ADAPT_DIM; ++j)
			coeff[i][j] = _coeff[i][j];

	thd_log_info("pid_adapt: restored coefficients from %s\n", name.c_str());
	return;

bad_file:
	thd_log_warn("pid_adapt: %s is corrupt, starting from unity trim\n",
			name.c_str());
}

bool cthd_pid_adaptive::store() const {
	if (key.empty())
		return false;

	std::ostringstream buf;

	buf << PID_ADAPT_FILE_VERSION;
	for (const auto &row : coeff)
		for (double w : row)
			buf << " " << w;
	buf << "\n";

	std::string name = pid_adapt_file_name(key);
	/* O_NOFOLLOW so a symlink planted in TDRUNDIR cannot redirect us. */
	int fd = ::open(name.c_str(),
			O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (fd < 0)
		return false;

	const std::string &data = buf.str();
	ssize_t w = ::write(fd, data.c_str(), data.size());
	::close(fd);

	return w == (ssize_t) data.size();
}

cthd_pid::cthd_pid() {
	kp = 0.0005;
	ki = kd = 0.0001;
	last_time = 0;
	err_sum = 0.0;
	last_err = 0.0;
	target_temp = 0;
	mode = PID_ABSOLUTE;
	adaptive = false;
}

void cthd_pid::set_pid_adaptive(bool enable, const std::string &_key) {
	adaptive = enable;

	if (!enable)
		return;

	trim_ctrl.reset();
	trim_ctrl.set_key(_key);

	thd_log_info("set_pid_adaptive key:%s kp:%g ki:%g kd:%g\n",
			_key.empty() ? "(memory only)" : _key.c_str(), kp, ki, kd);
}

double cthd_pid::adaptive_output(double error, double _err_sum, double d_err) {
	double X[PID_ADAPT_DIM] = { pid_adapt_norm(error, PID_ADAPT_ERR_REF),
			pid_adapt_norm(_err_sum, PID_ADAPT_INT_REF), pid_adapt_norm(d_err,
					PID_ADAPT_DERIV_REF) };
	double gsign[PID_ADAPT_DIM] = { (kp >= 0.0) ? 1.0 : -1.0, (ki >= 0.0) ? 1.0 :
			-1.0, (kd >= 0.0) ? 1.0 : -1.0 };
	double trim[PID_ADAPT_DIM], tanh_sum[PID_ADAPT_DIM];

	trim_ctrl.compute_trim(X, trim, tanh_sum);

	double output = kp * trim[0] * error + ki * trim[1] * _err_sum
			+ kd * trim[2] * d_err;

	trim_ctrl.adapt(X, trim, tanh_sum, gsign, X[0]);

	/*
	 * The normalised inputs are logged alongside the trim because that is
	 * what makes a bad adaptation diagnosable: a trim parked at a bound is
	 * almost always an input parked at +-1.
	 */
	thd_log_debug(
			"pid_adapt e:%.4f i:%.4f d:%.4f trim:%g,%g,%g kp:%g ki:%g kd:%g out:%g\n",
			X[0], X[1], X[2], trim[0], trim[1], trim[2], kp * trim[0],
			ki * trim[1], kd * trim[2], output);

	return output;
}

/*
 * Saturate to int before returning.  err_sum is unbounded -- it accumulates
 * error * dt for as long as the trip stays active -- so ki * err_sum can
 * exceed INT_MAX with entirely reasonable gains, and converting a double
 * that large to int is undefined behaviour rather than a large int.
 *
 * Saturating is the right answer rather than an error: the caller clamps to
 * the cooling device's own state range immediately afterwards, so INT_MAX
 * simply means "as much cooling as this device can do".
 */
static inline int pid_clamp_to_int(double v) {
	if (!std::isfinite(v))
		return 0;
	if (v > (double) INT_MAX)
		return INT_MAX;
	if (v < (double) INT_MIN)
		return INT_MIN;

	return (int) v;
}

int cthd_pid::pid_output(unsigned int curr_temp, int initial_value) {
	double output;
	/* Use signed arithmetic to avoid unsigned wrap-around when
	 * curr_temp < target_temp */
	int error = (int)curr_temp - (int)target_temp;

	time_t now;
	time(&now);

	if (last_time == 0) {
		/*
		 * First call: initialise state.  This deliberately uses the
		 * untrimmed gains even in adaptive mode, so the bumpless start
		 * below still lands exactly on initial_value when
		 * coefficients have been restored from a previous run.  Adaptation begins on the
		 * next poll, once there is a real dt to differentiate over.
		 */
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
		int out = pid_clamp_to_int(output);

		thd_log_debug("pid first call mode:%s e:%d out:%d\n",
				mode == PID_INCREMENTAL ? "inc" : "abs",
				error, out);
		return out;
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

	if (adaptive)
		output = adaptive_output(error, err_sum, d_err);
	else
		output = kp * error + ki * err_sum + kd * d_err;

	int out = pid_clamp_to_int(output);

	thd_log_debug("pid_%s%s e:%d kp:%g ki_sum:%g kd:%g out:%d\n",
			mode == PID_INCREMENTAL ? "inc" : "abs",
			adaptive ? ",adapt" : "",
			error, kp * error, ki * err_sum, kd * d_err, out);

	last_err = error;
	last_time = now;

	thd_log_debug("pid_output curr:%u tgt:%u mode:%d out:%d\n",
			curr_temp, target_temp, (int)mode, out);
	return out;
}
