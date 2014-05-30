/*
 * cthd_engine_defualt.cpp: Default thermal engine
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

#ifndef THD_ENGINE_DEFAULT_H_
#define THD_ENGINE_DEFAULT_H_

#include "thd_engine.h"
#include "thd_zone_surface.h"
#include "thd_cpu_default_binding.h"

class cthd_engine_default: public cthd_engine {
private:
	int parser_init();
	void parser_deinit();
	int add_replace_cdev(cooling_dev_t *config);

	bool parser_init_done;
	cthd_cpu_default_binding def_binding;

public:
	static const int power_clamp_reduction_percent = 5;

	cthd_engine_default() :
			cthd_engine(), parser_init_done(false) {
	}
	~cthd_engine_default();
	int read_thermal_zones();
	int read_cooling_devices();
	int read_thermal_sensors();
};

#endif /* THD_ENGINE_DEFAULT_H_ */
