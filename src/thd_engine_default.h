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
#include "thd_cpu_default_binding.h"
#include "thd_gddv.h"

class cthd_engine_default: public cthd_engine {
private:
	int add_replace_cdev(cooling_dev_t *config);
	bool add_int340x_processor_dev(void);
	void disable_cpu_zone(thermal_zone_t *zone_config);
	void workaround_rapl_mmio_power(void);
	void workaround_tcc_offset(void);

	cthd_cpu_default_binding def_binding;
	int workaround_interval;
#ifndef ANDROID
	int tcc_offset_checked;
	int tcc_offset_low;
#endif

protected:
	bool force_mmio_rapl;
public:
	static const int power_clamp_reduction_percent = 5;
	cthd_gddv gddv;

#ifndef ANDROID
	cthd_engine_default() :
			cthd_engine("42A441D6-AE6A-462b-A84B-4A8CE79027D3"),
			workaround_interval(0), tcc_offset_checked(0),
			tcc_offset_low(0), force_mmio_rapl(false) {
	}
	cthd_engine_default(std::string _uuid) :
			cthd_engine(std::move(_uuid)),
			workaround_interval(0), tcc_offset_checked(0),
			tcc_offset_low(0), force_mmio_rapl(false) {
	}
#else
	cthd_engine_default() :
			cthd_engine("42A441D6-AE6A-462b-A84B-4A8CE79027D3"),
			workaround_interval(0), force_mmio_rapl(false) {
	}
	cthd_engine_default(std::string _uuid) :
			cthd_engine(_uuid),
			workaround_interval(0), force_mmio_rapl(false) {
	}

#endif
	~cthd_engine_default();
	int read_thermal_zones();
	int read_cooling_devices();
	int read_thermal_sensors();
	void workarounds();
};

int thd_engine_create_default_engine(bool ignore_cpuid_check,
		bool exclusive_control, const char *config_file);
#endif /* THD_ENGINE_DEFAULT_H_ */
