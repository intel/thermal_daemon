/*
 * power_balancer.h: Power balancer interface file
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

#ifndef SRC_POWER_BALANCER_H_
#define SRC_POWER_BALANCER_H_

#include <string>
#include <vector>

typedef struct power_domain {
	int id;
	std::string name;
	double weight;
	double perf;
	double steady_state_power;
	double capped_power;
	double max_power;
	double min_power;
	double last_sample;
	double gap_to_max;
} power_domain_t;

typedef struct {
	double kp;
	double ki;
	double kd;
} pid_const_t;

class power_balancer {
private:
	int next_id;
	pid_const_t pid_param;
	std::vector<power_domain_t> power_domains;
	double err_sum, last_err;
	time_t last_time;
	double target;
	double target_low_limit;
	double pid_out;

	double pid_output(double current_input);
	void pid_reset();
public:
	power_balancer();
	virtual ~power_balancer();

	void set_pid_param(double kp, double ki, double kd);
	void set_pid_target(double _target);
	void reset();
	int add_new_power_domain(std::string name, double weight, double max_power,
			double min_power, double perf = 1.0);
	void remove_power_domain(int id);

	void set_power_target(double _target);
	void dump_domains();

	void add_new_sample(int id, double sample);
	void power_balance();
	double get_power(int id);
};

#endif /* SRC_POWER_BALANCER_H_ */
