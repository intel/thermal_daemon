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

#ifndef THD_ENGINE_ADAPTIVE_H_
#define THD_ENGINE_ADAPTIVE_H_

#ifndef ANDROID
#include <libevdev/libevdev.h>
#include <upower.h>
#endif

#include "thd_engine_default.h"
#include "thd_cpu_default_binding.h"
#include "thd_adaptive_types.h"


class cthd_engine_adaptive: public cthd_engine_default {
protected:
	int policy_active;
	int fallback_id;
	std::string int3400_path;
	std::string int3400_base_path;
	int passive_def_only;
	int passive_def_processed;
	int power_slider;
	int int3400_installed;
	int current_matched_target;

	int set_itmt_target(struct adaptive_target &target);
	int install_passive(struct psv *psv);
	int install_itmt(struct itmt_entry *itmt_entry);
	void psvt_consolidate();
	void set_trip(std::string device, std::string argument);
	void set_int3400_target(struct adaptive_target &target);
	void exec_fallback_target(int target);
	void execute_target(struct adaptive_target &target);
	void install_passive_default();

public:
#ifndef ANDROID
	cthd_engine_adaptive() :
			cthd_engine_default("63BE270F-1C11-48FD-A6F7-3AF253FF3E2D"), policy_active(
					0), fallback_id(-1), int3400_path(""), int3400_base_path(
					""), passive_def_only(0), passive_def_processed(0),
					power_slider(75), int3400_installed(0), current_matched_target(-1) {
	}
#else
	cthd_engine_adaptive() :
			cthd_engine_default("63BE270F-1C11-48FD-A6F7-3AF253FF3E2D"), policy_active(
					0), fallback_id(-1), int3400_base_path(""), passive_def_only(
					0), passive_def_processed(0) {
	}
#endif

	~cthd_engine_adaptive() {
	}
	ppcc_t* get_ppcc_param(std::string name) {
		return gddv.get_ppcc_param(std::move(name));
	}

	int search_idsp(std::string name)
	{
		return gddv.search_idsp(std::move(name));
	}

	int thd_engine_init(bool ignore_cpuid_check, bool adaptive);
	int thd_engine_start();
	void update_engine_state();
	void update_power_slider();
};

int thd_engine_create_adaptive_engine(bool ignore_cpuid_check, bool test_mode);
#endif /* THD_ENGINE_ADAPTIVE_H_ */
