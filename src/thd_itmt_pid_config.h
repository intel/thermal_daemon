/*
 * thd_itmt_pid_config.h: PID control of ITMT trips, enabled and tuned in
 * TDCONFDIR/itmt-pid.xml
 *
 * Copyright (C) 2026 Intel Corporation. All rights reserved.
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

#ifndef THD_ITMT_PID_CONFIG_H_
#define THD_ITMT_PID_CONFIG_H_

#include <string>

/*
 * With Enable 1 in itmt-pid.xml: ITMT entries only give a trip and a PL1 range, so they
 * are controlled by an incremental PID. Below the trip the limit is released
 * gradually. Without it, the default controller is used.
 *
 * The PID is time scaled, in velocity form: each call changes PL1 by
 * Kp * e * dt + Kd * (e - last e), e in millicelsius, dt in seconds,
 * PL1 in uW. So Kd acts as the proportional gain (1000: 1 W per C of
 * change) and Kp as the integral rate (30: 30 mW per second per C of
 * error), independent of the poll and sampling intervals.
 *
 * The default gains are for a common 25 W client system, PL1 range about
 * 17 W, with a skin sensor: first order with 0.75 C per W, a 60 s time
 * constant and a 20 s delay, read in whole degrees. With Kp 30 / Kd 1000
 * that loop has a 53 degree phase margin and 14.5 dB gain margin. Higher
 * gains (Kp 50 / Kd 2000: 46 degrees / 9 dB, Kp 100 / Kd 4000: 22 degrees /
 * 3 dB) overshot and oscillated more when tested. Skin sensors measured on
 * laptops (0.44-1.11 C per W, 20-180 s, 0-45 s delay) give 47-65 degrees.
 *
 * The gains are scaled with the PL1 range of the ITMT entry relative to
 * that reference, so that the limit crosses the range in about the same
 * time on smaller and larger systems, within 0.5x-1.5x: above about 1.5x a
 * skin sensor with a long delay loses most of its phase margin (40 degrees
 * at 1.5x, 12 at 2x for a 45 s delay).
 *
 * The set point is half a degree below the trip: a whole degree sensor
 * reading e.g. 59.05 C at a 59 C trip would give +0.05 C at the trip and
 * -0.95 C one degree below, throttling 19 times weaker than releasing;
 * at 58.5 C the error is about +-0.5 C either way.
 *
 * TDCONFDIR/itmt-pid.xml, all elements optional:
 *
 *	<ItmtPid>
 *		<Enable>1</Enable>		0 or missing: default controller
 *		<Kp>30</Kp>
 *		<Kd>1000</Kd>
 *		<SetPointOffset>500</SetPointOffset>	millicelsius
 *		<ScaleWithRange>1</ScaleWithRange>	0: Kp and Kd as given
 *		<ReferenceRange>17000</ReferenceRange>	mW
 *		<ScaleMin>0.5</ScaleMin>
 *		<ScaleMax>1.5</ScaleMax>
 *	</ItmtPid>
 */
#define ITMT_PID_KP			30.0
#define ITMT_PID_KD			1000.0
#define ITMT_PID_SET_POINT_OFFSET	500
#define ITMT_PID_REFERENCE_RANGE_MW	17000
#define ITMT_PID_SCALE_MIN		0.5
#define ITMT_PID_SCALE_MAX		1.5

class cthd_itmt_pid_config {
public:
	bool enable = false;
	double kp = ITMT_PID_KP;
	double kd = ITMT_PID_KD;
	int set_point_offset = ITMT_PID_SET_POINT_OFFSET;
	bool scale_with_range = true;
	int reference_range_mw = ITMT_PID_REFERENCE_RANGE_MW;
	double scale_min = ITMT_PID_SCALE_MIN;
	double scale_max = ITMT_PID_SCALE_MAX;

	/* Read TDCONFDIR/itmt-pid.xml if present; disabled, defaults otherwise */
	void load();
	void load(const std::string &filename);

	/* Gain scale for a PL1 range in uW */
	double scale(long long range_uw) const;
};

#endif /* THD_ITMT_PID_CONFIG_H_ */
