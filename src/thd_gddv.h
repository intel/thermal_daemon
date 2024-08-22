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
 * Author Name Matthew Garrett <mjg59@google.com>
 *
 */

#ifndef THD_GDDV_H_
#define THD_GDDV_H_

#ifndef ANDROID
#include <libevdev/libevdev.h>
#include <upower.h>
#endif

#include "thd_engine.h"
#include "thd_trt_art_reader.h"

#define DECI_KELVIN_TO_CELSIUS(t)       ({                      \
        int _t = (t);                                          \
        ((_t-2732 >= 0) ? (_t-2732+5)/10 : (_t-2732-5)/10);     \
})

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
	Unk1,
	Unk2,
	Battery_state,
	Battery_rate,
	Battery_remaining,
	Battery_voltage,
	PBSS,
	Battery_cycles,
	Battery_last_full,
	Power_personality,
	Battery_design_capacity,
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
	ADAPTIVE_EQUAL = 0x01, ADAPTIVE_LESSER_OR_EQUAL, ADAPTIVE_GREATER_OR_EQUAL,
};

enum adaptive_operation {
	AND = 0x01, FOR
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
	uint64_t condition;
	std::string device;
	uint64_t comparison;
	int argument;
	enum adaptive_operation operation;
	enum adaptive_comparison time_comparison;
	time_t time;
	int target;
	int state;
	time_t state_entry_time;
	int ignore_condition;
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

struct itmt_entry {
	std::string target;
	int trip_point;
	std::string pl1_min;
	std::string pl1_max;
	std::string unused;
};

struct trt_entry {
	std::string source;
	std::string dest;
	int priority;
	int sample_rate;
	int resd0;
	int resd1;
	int resd2;
	int resd3;
};

struct itmt {
	std::string name;
	std::vector<struct itmt_entry> itmt_entries;
};

struct trippoint {
	std::string name;
	std::string type_str;
	trip_point_type_t type;
	int temp;
};

class cthd_gddv {
private:
	std::vector<ppcc_t> ppccs;
	std::vector<struct custom_condition> custom_conditions;
	std::vector<rel_object_t> rel_list;
	std::vector<struct psvt> psvts;
	std::vector<struct itmt> itmts;
	std::vector<std::string> idsps;
	std::vector<struct trippoint> trippoints;
	std::string int3400_path;
#ifndef ANDROID
	UpClient *upower_client;
	GDBusProxy *power_profiles_daemon;
	struct libevdev *tablet_dev;
	struct libevdev *lid_dev;
#endif
	std::string int3400_base_path;
	int power_slider;

	void destroy_dynamic_sources();
	int get_type(char *object, int *offset);
	uint64_t get_uint64(char *object, int *offset);
	char* get_string(char *object, int *offset);
	int merge_custom(struct custom_condition *custom,
			struct condition *condition);
	int merge_appc(void);
	int parse_appc(char *appc, int len);
	int parse_apat(char *apat, int len);
	int parse_apct(char *apct, int len);
	int parse_ppcc(char *name, char *ppcc, int len);
	int parse_psvt(char *name, char *psvt, int len);
	int parse_itmt(char *name, char *itmt, int len);
	int parse_trt(char *trt, int len);
	void parse_idsp(char *name, char *idsp, int len);
	void parse_trip_point(char *name, char *type, char *val, int len);
	int handle_compressed_gddv(char *buf, int size);
	int parse_gddv_key(char *buf, int size, int *end_offset);
	int parse_gddv(char *buf, int size, int *end_offset);
	int verify_condition(struct condition condition);
	int compare_condition(struct condition condition, int value);
	int compare_time(struct condition condition);
	int evaluate_oem_condition(struct condition condition);
	int evaluate_temperature_condition(struct condition condition);
	int evaluate_ac_condition(struct condition condition);
	int evaluate_lid_condition(struct condition condition);
	int evaluate_workload_condition(struct condition condition);
	int evaluate_platform_type_condition(struct condition condition);
	int evaluate_power_slider_condition(struct condition condition);
	int evaluate_condition(struct condition condition);
	int evaluate_condition_set(std::vector<struct condition> condition_set);
	void exec_fallback_target(int target);
	void dump_apat();
	void dump_apct();
	void dump_ppcc();
	void dump_psvt();
	void dump_itmt();
	void dump_idsps();
	void dump_trips();
#ifndef ANDROID
	void setup_input_devices();
#endif
	int get_trip_temp(std::string name, trip_point_type_t type);

public:
#ifndef ANDROID
	cthd_gddv() :
			upower_client(
			NULL), power_profiles_daemon(NULL), tablet_dev(NULL), lid_dev(NULL), int3400_base_path(""), power_slider(75) {
	}
#else
	cthd_gddv() :
			int3400_base_path(""), current_condition_set(0xffff) {
	}

#endif
	~cthd_gddv();

	std::vector<std::vector<struct condition>> conditions;
	std::vector<struct adaptive_target> targets;

	ppcc_t* get_ppcc_param(std::string name);
	int gddv_init(void);
	size_t gddv_load(char **buf);
	void gddv_free(void);
	int verify_conditions();
	int evaluate_conditions();
	void update_power_slider();
	int find_agressive_target();
	struct psvt* find_psvt(std::string name);
	struct itmt* find_itmt(std::string name);
	struct psvt* find_def_psvt();
	int search_idsp(std::string name);

};

#endif /* THD_GDDV_H_ */
