/*
 * power_balancer.cpp: Power balancer implementation file
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
#include "power_balancer.h"
#include <algorithm>

#ifdef DEBUG_PWR_BALANCER
#define DEBUG_PRINTF printf
#else
#define DEBUG_PRINTF(...)
#endif

power_balancer::power_balancer() :
		next_id(0), err_sum(0), target(0), target_low_limit(0), pid_out(0) {
	pid_param.kp = 0.1;
	pid_param.ki = 0;
	pid_param.kd = 0;
}

power_balancer::~power_balancer() {
}

void power_balancer::set_pid_param(double kp, double ki, double kd) {
	pid_param.kp = kp;
	pid_param.ki = ki;
	pid_param.kd = kd;
}

void power_balancer::set_pid_target(double _target) {
	target = _target;
	target_low_limit = target - (target * 10 / 100);
}

double power_balancer::pid_output(double current_input) {
	double output;
	double d_err = 0;

	time_t now;
	time(&now);
	if (last_time == 0)
		last_time = now;
	time_t timeChange = (now - last_time);

	int error = current_input - target;

	err_sum += (error * timeChange);
	if (timeChange)
		d_err = (error - last_err) / timeChange;
	else
		d_err = 0.0;

	/*Compute PID Output*/
	output = pid_param.kp * error + pid_param.ki * err_sum
			+ pid_param.kd * d_err;

	/*Remember some variables for next time*/
	last_err = error;
	last_time = now;
	DEBUG_PRINTF("pid_output %g:%g %g:%d\n", current_input, target, output,
			(int) output);
	return output;
}

void power_balancer::pid_reset()
{
	err_sum = last_err = last_time = 0;
}

void power_balancer::reset()
{
	pid_reset();
}

static bool sort_weight_values_dec(power_domain_t pd1, power_domain_t pd2) {
	return (pd1.weight > pd2.weight);
}

int power_balancer::add_new_power_domain(std::string name, double weight,
		double max_power, double min_power) {
	power_domain_t pd;

	pd.id = next_id++;
	pd.name = name;
	pd.weight = weight;
	pd.max_power = max_power;
	pd.min_power = min_power;
	pd.steady_state_power = 0;
	pd.capped_power = max_power;
	pd.last_sample = 0;
	power_domains.push_back(pd);

	std::sort(power_domains.begin(), power_domains.end(),
			sort_weight_values_dec);
	return pd.id;
}

void power_balancer::remove_power_domain(int id) {
	size_t i;

	for (i = 0; i < power_domains.size(); ++i) {
		if (power_domains[i].id == id)
			power_domains.erase(power_domains.begin() + i);
	}
}

void power_balancer::dump_domains() {
	size_t i;

	for (i = 0; i < power_domains.size(); ++i) {
		printf("id:%d name:%s\n", power_domains[i].id,
				power_domains[i].name.c_str());
	}
}

void power_balancer::set_power_target(double _target) {
	target = _target;
}

void power_balancer::add_new_sample(int id, double sample) {
	size_t i;

	for (i = 0; i < power_domains.size(); ++i) {
		if (power_domains[i].id == id) {
			power_domains[i].last_sample = sample;
			if (sample < power_domains[i].steady_state_power)
				power_domains[i].steady_state_power = sample;
		}
	}
}

// Similar to kernel drivers/thermal/power_allocator.c
// In longer run this thermal power_allocator governor will be
// reused when this logic moves to linux kernel powecap
// with some changes in allocation style.
void power_balancer::power_balance() {
	size_t i;
	int total_power = 0;

	for (i = 0; i < power_domains.size(); ++i) {
		total_power += power_domains[i].last_sample;
	}

	pid_out = pid_output(total_power);

	double surplus = 0;
	double total_power_gap = 0;

	DEBUG_PRINTF("new power cap target %g\n", target + pid_out);
	for (i = 0; i < power_domains.size(); ++i) {
		double share = (target + pid_out) * power_domains[i].weight;

		DEBUG_PRINTF("domain:%lu last_sample:%g current_share %g\n", i,
				power_domains[i].last_sample, share);
		if (power_domains[i].last_sample <= share) {
			power_domains[i].capped_power = power_domains[i].last_sample;
			surplus += (share - power_domains[i].last_sample);
			power_domains[i].gap_to_max = 0;
			continue;
		}

		power_domains[i].capped_power = share;

		if (power_domains[i].capped_power > power_domains[i].max_power) {
			surplus += (power_domains[i].max_power
					- power_domains[i].capped_power);
			power_domains[i].capped_power = power_domains[i].max_power;
		}
		power_domains[i].gap_to_max = power_domains[i].max_power
				- power_domains[i].capped_power;
		total_power_gap += power_domains[i].gap_to_max;
	}

	if (surplus) {
		for (i = 0; i < power_domains.size(); ++i) {
			double extra;

			DEBUG_PRINTF("domain %lu gap to max %g\n", i,
					power_domains[i].gap_to_max);
			if (power_domains[i].gap_to_max == 0)
				continue;

			extra = surplus * power_domains[i].gap_to_max / total_power_gap;

			DEBUG_PRINTF("surplus %g i=%lu, capped_power=%g extra=%g\n", surplus, i,
					power_domains[i].capped_power, extra);

			if (power_domains[i].capped_power + extra
					< power_domains[i].max_power) {
				power_domains[i].capped_power += extra;
			}

			if (power_domains[i].capped_power > power_domains[i].max_power) {
				surplus += (power_domains[i].max_power
						- power_domains[i].capped_power);
				power_domains[i].capped_power = power_domains[i].max_power;
			}
		}
	}
}

double power_balancer::get_power(int id) {
	size_t i;

	for (i = 0; i < power_domains.size(); ++i) {
		if (power_domains[i].id == id) {
			double pwr = power_domains[i].capped_power;
			DEBUG_PRINTF("i:%lu pwr %g [%g:%g]\n", i, pwr,
					power_domains[i].last_sample,
					target * power_domains[i].weight);

			if (power_domains[i].last_sample
					<= target * power_domains[i].weight) {
				pwr = target * power_domains[i].weight;
			}
			return pwr;
		}
	}

	return 0;
}

#ifdef DEBUG_PWR_BALANCER
void sample_usage()
{
	power_balancer pwr_balancer;

	pwr_balancer.add_new_power_domain("amd-gpu", 0.5, 65, 5);
	pwr_balancer.add_new_power_domain("pkg-rapl", 0.3, 65, 20);
	pwr_balancer.add_new_power_domain("modem", 0.2, 65, 5);
	pwr_balancer.set_pid_param(-0.0, 0, 0);
	pwr_balancer.set_pid_target(50);
	pwr_balancer.dump_domains();

	pwr_balancer.add_new_sample(0, 40.0);
	pwr_balancer.add_new_sample(1, 30.0);
	pwr_balancer.add_new_sample(2, 10.0);
	pwr_balancer.power_balance();
	printf("id 0: power:%g\n", pwr_balancer.get_power(0));
	printf("id 1: power:%g\n", pwr_balancer.get_power(1));
	printf("id 2: power:%g\n", pwr_balancer.get_power(2));

	pwr_balancer.add_new_sample(0, 40.0);
	pwr_balancer.add_new_sample(1, 30.0);
	pwr_balancer.add_new_sample(2, 20.0);
	pwr_balancer.power_balance();
	printf("id 0: power:%g\n", pwr_balancer.get_power(0));
	printf("id 1: power:%g\n", pwr_balancer.get_power(1));
	printf("id 2: power:%g\n", pwr_balancer.get_power(2));

	pwr_balancer.add_new_sample(0, 40.0);
	pwr_balancer.add_new_sample(1, 30.0);
	pwr_balancer.add_new_sample(2, 5.0);
	pwr_balancer.power_balance();
	printf("id 0: power:%g\n", pwr_balancer.get_power(0));
	printf("id 1: power:%g\n", pwr_balancer.get_power(1));
	printf("id 2: power:%g\n", pwr_balancer.get_power(2));
}
#endif

