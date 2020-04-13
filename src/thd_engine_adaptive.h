/*
 * cthd_engine_adaptive.cpp: Adaptive thermal engine
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright 2020 Google LLC
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
 * Author Name Matthew Garrett <mjg59@google.com>
 *
 */

#ifndef THD_ENGINE_ADAPTIVE_H_
#define THD_ENGINE_ADAPTIVE_H_

#include <libevdev/libevdev.h>
#include <upower.h>

#include "thd_engine_default.h"
#include "thd_cpu_default_binding.h"
#include "thd_adaptive_types.h"

enum adaptive_condition {
	Default = 0x01,
	Orientation,
	Proximity,
	Motion,
	Dock,
	Workload,
	Cooling_mode,
	Power_source,
	Aggregate_power_percentage,
	Lid_state,
	Platform_type,
	Platform_SKU,
	Utilisation,
	TDP,
	Duty_cycle,
	Power,
	Temperature,
	Display_orientation,
	Oem0,
	Oem1,
	Oem2,
	Oem3,
	Oem4,
	Oem5,
	PMAX,
	PSRC,
	ARTG,
	CTYP,
	PROP,
	Battery_state,
	Battery_rate,
	Battery_remaining,
	Battery_voltage,
	PBSS,
	Battery_cycles,
	Battery_last_full,
	Power_personality,
	Screen_state,
	AVOL,
	ACUR,
	AP01,
	AP02,
	AP10,
	Time,
	Temperature_without_hysteresis,
	Mixed_reality,
	User_presence,
	RBHF,
	VBNL,
	CMPP,
	Battery_percentage,
	Battery_count,
	Power_slider
};

enum adaptive_comparison {
	ADAPTIVE_EQUAL = 0x01,
	ADAPTIVE_LESSER_OR_EQUAL,
	ADAPTIVE_GREATER_OR_EQUAL,
};

enum adaptive_operation {
	AND = 0x01,
	FOR
};

struct psv {
	std::string name;
	std::string source;
	std::string target;
	int priority;
	int sample_period;
	int temp;
	int domain;
	int control_knob;
	std::string limit;
	int step_size;
	int limit_coeff;
	int unlimit_coeff;
};

struct condition {
	enum adaptive_condition condition;
	std::string device;
        enum adaptive_comparison comparison;
        int argument;
	enum adaptive_operation operation;
        enum adaptive_comparison time_comparison;
        int time;
	int target;
	int state;
	int state_entry_time;
};

struct custom_condition {
	enum adaptive_condition condition;
	std::string name;
	std::string participant;
	int domain;
	int type;
};

struct psvt {
	std::string name;
	std::vector<struct psv> psvs;
};

class cthd_engine_adaptive: public cthd_engine_default {
protected:
	std::vector<ppcc_t> ppccs;
	std::vector<std::vector<struct condition>> conditions;
	std::vector<struct custom_condition> custom_conditions;
	std::vector<struct adaptive_target> targets;
	std::vector<struct psvt> psvts;
	std::string int3400_path;
	UpClient *upower_client;
	struct libevdev *tablet_dev;
	int get_type(char *object, int *offset);
	uint64_t get_uint64(char *object, int *offset);
	char *get_string(char *object, int *offset);
	int merge_custom(struct custom_condition *custom, struct condition *condition);
	int merge_appc(void);
	int parse_appc(char *appc, int len);
	int parse_apat(char *apat, int len);
	int parse_apct(char *apct, int len);
	int parse_ppcc(char *name, char *ppcc, int len);
	int parse_psvt(char *name, char *psvt, int len);
	int handle_compressed_gddv(char *buf, int size);
	int parse_gddv(char *buf, int size);
	struct psvt *find_psvt(std::string name);
	int install_passive(struct psv *psv);
	void set_int3400_target(struct adaptive_target target);
	int verify_condition(struct condition condition);
	int verify_conditions();
	int compare_condition(struct condition condition, int value);
	int evaluate_oem_condition(struct condition condition);
	int evaluate_temperature_condition(struct condition condition);
	int evaluate_ac_condition(struct condition condition);
	int evaluate_lid_condition(struct condition condition);
	int evaluate_workload_condition(struct condition condition);
	int evaluate_platform_type_condition(struct condition condition);
	int evaluate_condition(struct condition condition);
	int evaluate_condition_set(std::vector<struct condition> condition_set);
	int evaluate_conditions();
	void execute_target(struct adaptive_target target);
	void setup_input_devices();
public:
	cthd_engine_adaptive() :
		cthd_engine_default("63BE270F-1C11-48FD-A6F7-3AF253FF3E2D") {
	}

	~cthd_engine_adaptive();
	ppcc_t *get_ppcc_param(std::string name);
	int thd_engine_start(bool ignore_cpuid_check);
	void update_engine_state();
};

int thd_engine_create_adaptive_engine(bool ignore_cpuid_check);
#endif /* THD_ENGINE_ADAPTIVE_H_ */
