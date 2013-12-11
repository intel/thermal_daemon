/* thd_msr.cpp: thermal engine msr class implementation
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

/* Abstracts all the MSR accesses for the thermal daemon. There shouldn't be any
 * MSR access other module.
 *
 */

#include "thd_msr.h"

// MSR defines used in this module
#define MSR_IA32_MISC_ENABLE	0x000001a0
#define MSR_IA32_MISC_ENABLE_TURBO_DISABLE      (1ULL << 38)

#define MSR_IA32_THERM_CONTROL          		0x0000019a

#define MSR_IA32_CLK_MOD_ENABLE					(1ULL << 4)
#define MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK		(0xF)

#define MSR_IA32_PERF_CTL						0x0199
#define TURBO_DISENGAGE_BIT						(1ULL << 32)
#define PERF_CTL_CLK_SHIFT						(8)
#define PERF_CTL_CLK_MASK						(0xff00)

#define IA32_ENERGY_PERF_BIAS					0x01B0

#define MSR_IA32_PLATFORM_INFO					0xCE
#define MSR_IA32_PLATFORM_INFO_MIN_FREQ(value)	((value >> 40) & 0xFF)
#define MSR_IA32_PLATFORM_INFO_MAX_FREQ(value)	((value >> 8) & 0xFF)

#define MSR_TURBO_RATIO_LIMIT					0x1AD
#define TURBO_RATIO_1C_MASK						0xFF
#define TURBO_RATIO_2C_MASK						0xFF00
#define TURBO_RATIO_2C_SHIFT					(8)
#define TURBO_RATIO_3C_MASK						0xFF0000
#define TURBO_RATIO_3C_SHIFT					(16)
#define TURBO_RATIO_4C_MASK						0xFF000000
#define TURBO_RATIO_4C_SHIFT					(24)

#define MSR_IA32_APERF							0xE8
#define MSR_IA32_MPERF							0xE7

cthd_msr::cthd_msr() :
		msr_sysfs("/dev/cpu/"), no_of_cpus(0) {
}

int cthd_msr::read_msr(int cpu, unsigned int idx, unsigned long long *val) {
	int ret = -1;
	std::stringstream file_name_str;

	file_name_str << cpu << "/msr";
	if (msr_sysfs.exists(file_name_str.str())) {
		ret = msr_sysfs.read(file_name_str.str(), idx, (char*) val,
				sizeof(*val));
	}
	if (ret < 0) {
		thd_log_warn("MSR READ Failed \n");
	}

	return ret;
}

int cthd_msr::write_msr(int cpu, unsigned int idx, unsigned long long val) {
	int ret = -1;
	std::stringstream file_name_str;
	file_name_str << cpu << "/msr";
	if (msr_sysfs.exists(file_name_str.str())) {
		ret = msr_sysfs.write(file_name_str.str(), idx, val);
	}
	if (ret < 0) {
		thd_log_warn("MSR WRITE Failed \n");
	}

	return ret;
}

int cthd_msr::get_no_cpus() {
	int count = 0;

	if (no_of_cpus)
		return no_of_cpus;

	for (int i = 0; i < 64; ++i) {
		std::stringstream file_name_str;

		file_name_str << i;
		if (msr_sysfs.exists(file_name_str.str())) {
			count++;
		}
	}
	no_of_cpus = count;

	return count;
}

bool cthd_msr::check_turbo_status() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	// Return false even if one of the core is not enabled
	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_MISC_ENABLE, &val);
		if (ret < 0)
			return false;

		if (val & MSR_IA32_MISC_ENABLE_TURBO_DISABLE)
			return false;
	}

	return true;
}

int cthd_msr::enable_turbo_per_cpu(int cpu) {
	unsigned long long val;
	int ret;

	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	if (ret < 0)
		return THD_ERROR;

	val &= ~TURBO_DISENGAGE_BIT;
	ret = write_msr(cpu, MSR_IA32_PERF_CTL, val);
	if (ret < 0)
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_msr::enable_turbo() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	for (int i = 0; i < cpu_count; ++i) {
		/*
		 This method is recommended to be used only in BIOS

		 ret = read_msr(i, MSR_IA32_MISC_ENABLE, &val);
		 if (ret < 0)
		 return THD_ERROR;
		 val &= ~MSR_IA32_MISC_ENABLE_TURBO_DISABLE;

		 ret = write_msr(i, MSR_IA32_MISC_ENABLE, val);
		 if (ret < 0)
		 return THD_ERROR;
		 */
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		if (ret < 0)
			return THD_ERROR;

		val &= ~TURBO_DISENGAGE_BIT;
		ret = write_msr(i, MSR_IA32_PERF_CTL, val);
		if (ret < 0)
			return THD_ERROR;
	}

	thd_log_info("Turbo enabled \n");

	return THD_SUCCESS;
}

int cthd_msr::disable_turbo_per_cpu(int cpu) {
	unsigned long long val;
	int ret;

	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	if (ret < 0)
		return THD_ERROR;
	val |= TURBO_DISENGAGE_BIT;
	ret = write_msr(cpu, MSR_IA32_PERF_CTL, val);
	if (ret < 0)
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_msr::disable_turbo() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	for (int i = 0; i < cpu_count; ++i) {
		/*
		 This method is recommended only for BIOS

		 ret = read_msr(i, MSR_IA32_MISC_ENABLE, &val);
		 if (ret < 0)
		 return THD_ERROR;
		 val |= MSR_IA32_MISC_ENABLE_TURBO_DISABLE;

		 ret = write_msr(i, MSR_IA32_MISC_ENABLE, val);
		 if (ret < 0)
		 return THD_ERROR;
		 */
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		if (ret < 0)
			return THD_ERROR;
		val |= TURBO_DISENGAGE_BIT;
		ret = write_msr(i, MSR_IA32_PERF_CTL, val);
		if (ret < 0)
			return THD_ERROR;
	}
	thd_log_info("Turbo disabled \n");

	return THD_SUCCESS;
}

int cthd_msr::set_clock_mod_duty_cycle_per_cpu(int cpu, int state) {
	unsigned long long val;
	int ret;

	// First bit is reserved
	state = state << 1;

	ret = read_msr(cpu, MSR_IA32_THERM_CONTROL, &val);
	if (ret < 0)
		return THD_ERROR;

	if (!state) {
		val &= ~MSR_IA32_CLK_MOD_ENABLE;
	} else {
		val |= MSR_IA32_CLK_MOD_ENABLE;
	}
	val &= ~MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK;
	val |= (state & MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK);

	ret = write_msr(cpu, MSR_IA32_THERM_CONTROL, val);
	if (ret < 0) {
		thd_log_warn("set_clock_mod_duty_cycle current set failed to write\n");
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_msr::set_clock_mod_duty_cycle(int state) {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("Set T stated %d \n", state);
	// First bit is reserved
	state = state << 1;

	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_THERM_CONTROL, &val);
		thd_log_debug("set_clock_mod_duty_cycle current %x\n",
				(unsigned int) val);
		if (ret < 0)
			return THD_ERROR;

		if (!state) {
			val &= ~MSR_IA32_CLK_MOD_ENABLE;
		} else {
			val |= MSR_IA32_CLK_MOD_ENABLE;
		}

		val &= ~MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK;
		val |= (state & MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK);

		thd_log_debug("set_clock_mod_duty_cycle current set to %x\n",
				(unsigned int) val);
		ret = write_msr(i, MSR_IA32_THERM_CONTROL, val);
		if (ret < 0) {
			thd_log_warn(
					"set_clock_mod_duty_cycle current set failed to write\n");
			return THD_ERROR;
		}
	}

	return THD_SUCCESS;
}

int cthd_msr::get_clock_mod_duty_cycle() {
	int ret, state = 0;
	unsigned long long val;

	// Just get for cpu 0 and return
	ret = read_msr(0, MSR_IA32_THERM_CONTROL, &val);
	thd_log_debug("get_clock_mod_duty_cycle current %x\n", (unsigned int) val);
	if (ret < 0)
		return THD_ERROR;

	if (val & MSR_IA32_CLK_MOD_ENABLE) {
		state = val & MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK;
		state = state >> 1;
		thd_log_debug("current state  %x\n", state);
	}

	return state;
}

int cthd_msr::get_min_freq() {
	int ret;
	unsigned long long val;

	// Just get for cpu 0 and return
	ret = read_msr(0, MSR_IA32_PLATFORM_INFO, &val);
	if (ret < 0)
		return THD_ERROR;

	return MSR_IA32_PLATFORM_INFO_MIN_FREQ(val);
}

int cthd_msr::get_min_turbo_freq() {
	int ret;
	unsigned long long val;

	// Read turbo ratios
	ret = read_msr(0, MSR_TURBO_RATIO_LIMIT, &val);
	if (ret < 0)
		return THD_ERROR;
	// We are in a thermal zone. that means we already running all cores at full speed
	// So take the value for all 4 cores running
	val &= TURBO_RATIO_4C_MASK;
	if (val)
		return val >> TURBO_RATIO_4C_SHIFT;

	return 0;
}

int cthd_msr::get_max_turbo_freq() {
	int ret;
	unsigned long long val;

	// Read turbo ratios
	ret = read_msr(0, MSR_TURBO_RATIO_LIMIT, &val);
	if (ret < 0)
		return THD_ERROR;
	// We are in a thermal zone. that means we already running all cores at full speed
	// So take the value for all 4 cores running
	val &= 0xff;

	return val;
}

int cthd_msr::get_max_freq() {
	int ret;
	unsigned long long val;

	ret = read_msr(0, MSR_IA32_PLATFORM_INFO, &val);
	if (ret < 0)
		return THD_ERROR;

	return MSR_IA32_PLATFORM_INFO_MAX_FREQ(val);
}

int cthd_msr::dec_freq_state_per_cpu(int cpu) {
	unsigned long long val;
	int ret;
	int current_clock;

	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	thd_log_debug("perf_ctl current %x\n", (unsigned int) val);
	if (ret < 0)
		return THD_ERROR;

	current_clock = (val >> 8) & 0xff;
	current_clock--;

	val = (current_clock << PERF_CTL_CLK_SHIFT);

	thd_log_debug("perf_ctl write %x\n", (unsigned int) val);
	ret = write_msr(cpu, MSR_IA32_PERF_CTL, val);
	if (ret < 0) {
		thd_log_warn("per control msr failded to write\n");
		return THD_ERROR;
	}
	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	thd_log_debug("perf_ctl read back %x\n", (unsigned int) val);
	if (ret < 0)
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_msr::dec_freq_state() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;
	int current_clock;

	thd_log_info("dec freq \n");

	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		thd_log_debug("perf_ctl current %x\n", (unsigned int) val);
		if (ret < 0)
			return THD_ERROR;

		current_clock = (val >> 8) & 0xff;
		current_clock--;

		val = (current_clock << PERF_CTL_CLK_SHIFT);

		thd_log_debug("perf_ctl write %x\n", (unsigned int) val);
		ret = write_msr(i, MSR_IA32_PERF_CTL, val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to write\n");
			return THD_ERROR;
		}
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		thd_log_debug("perf_ctl read back %x\n", (unsigned int) val);
		if (ret < 0)
			return THD_ERROR;

	}

	return THD_SUCCESS;
}

int cthd_msr::inc_freq_state_per_cpu(int cpu) {
	unsigned long long val;
	int ret;
	int current_clock;

	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	if (ret < 0)
		return THD_ERROR;

	current_clock = (val >> 8) & 0xff;
	current_clock++;

	val = (current_clock << PERF_CTL_CLK_SHIFT);

	ret = write_msr(cpu, MSR_IA32_PERF_CTL, val);
	if (ret < 0) {
		thd_log_warn("per control msr failded to write\n");
		return THD_ERROR;
	}
	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	thd_log_debug("perf_ctl read back %x\n", (unsigned int) val);
	if (ret < 0)
		return THD_ERROR;

	return THD_SUCCESS;
}

int cthd_msr::inc_freq_state() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;
	int current_clock;

	thd_log_info("inc freq state \n");
	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		if (ret < 0)
			return THD_ERROR;
		thd_log_debug("perf_ctl current %x\n", (unsigned int) val);
		if (ret < 0)
			return THD_ERROR;

		current_clock = (val >> 8) & 0xff;
		current_clock++;

		val = (current_clock << PERF_CTL_CLK_SHIFT);

		thd_log_debug("perf_ctl write %x\n", (unsigned int) val);
		ret = write_msr(i, MSR_IA32_PERF_CTL, val);
		if (ret < 0) {
			thd_log_warn("per control msr failded to write\n");
			return THD_ERROR;
		}
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		thd_log_debug("perf_ctl read back %x\n", (unsigned int) val);
		if (ret < 0)
			return THD_ERROR;

	}

	return THD_SUCCESS;
}

int cthd_msr::set_freq_state_per_cpu(int cpu, int state) {
	unsigned long long val;
	int ret;

	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	if (ret < 0)
		return THD_ERROR;
	val &= ~PERF_CTL_CLK_MASK;
	val |= (state << PERF_CTL_CLK_SHIFT);
	thd_log_debug("perf_ctl current %x\n", (unsigned int) val);
	ret = write_msr(cpu, MSR_IA32_PERF_CTL, val);
	if (ret < 0) {
		thd_log_warn("per control msr failded to write\n");
		return THD_ERROR;
	}
#ifdef READ_BACK_VERIFY
	ret = read_msr(cpu, MSR_IA32_PERF_CTL, &val);
	thd_log_debug("perf_ctl read back %x\n", val);
	if(ret < 0)
	return THD_ERROR;
#endif
	return THD_SUCCESS;
}

int cthd_msr::set_freq_state(int state) {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("set_freq_state \n");
	for (int i = 0; i < cpu_count; ++i) {

		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		if (ret < 0)
			return THD_ERROR;

		val &= ~PERF_CTL_CLK_MASK;
		val |= (state << PERF_CTL_CLK_SHIFT);

		thd_log_debug("perf_ctl write %x\n", (unsigned int) val);
		ret = write_msr(i, MSR_IA32_PERF_CTL, val);
		if (ret < 0) {
			thd_log_warn("per control msr failded to write\n");
			return THD_ERROR;
		}
#ifdef READ_BACK_VERIFY
		ret = read_msr(i, MSR_IA32_PERF_CTL, &val);
		thd_log_debug("perf_ctl read back %x\n", val);
		if(ret < 0)
		return THD_ERROR;
#endif
	}

	return THD_SUCCESS;
}

int cthd_msr::set_perf_bias_performace() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("set_perf_bias_performace \n");
	val = 0;
	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, IA32_ENERGY_PERF_BIAS, &val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to read\n");
			return THD_ERROR;
		}
		val &= ~0x0f;
		ret = write_msr(i, IA32_ENERGY_PERF_BIAS, val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to write\n");
			return THD_ERROR;
		}
	}
	return THD_SUCCESS;
}

int cthd_msr::set_perf_bias_balaced() {
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("set_perf_bias_balaced \n");
	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, IA32_ENERGY_PERF_BIAS, &val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to read\n");
			return THD_ERROR;
		}
		val &= ~0x0f;
		val |= 6;
		ret = write_msr(i, IA32_ENERGY_PERF_BIAS, val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to write\n");
			return THD_ERROR;
		}
	}
	return THD_SUCCESS;
}

int cthd_msr::set_perf_bias_energy() {

	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("set_perf_bias_energy \n");
	for (int i = 0; i < cpu_count; ++i) {
		ret = read_msr(i, IA32_ENERGY_PERF_BIAS, &val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to read\n");
			return THD_ERROR;
		}
		val &= ~0x0f;
		val |= 15;
		ret = write_msr(i, IA32_ENERGY_PERF_BIAS, val);
		if (ret < 0) {
			thd_log_warn("per control msr failed to write\n");
			return THD_ERROR;
		}
	}
	return THD_SUCCESS;
}

int cthd_msr::get_mperf_value(int cpu, unsigned long long *value) {
	int ret;

	if (cpu >= get_no_cpus())
		return THD_ERROR;
	ret = read_msr(cpu, MSR_IA32_MPERF, value);
	if (ret < 0) {
		thd_log_warn("get_mperf_value failed to read\n");
		return THD_ERROR;
	}
	return THD_SUCCESS;
}

int cthd_msr::get_aperf_value(int cpu, unsigned long long *value) {
	int ret;

	if (cpu >= get_no_cpus())
		return THD_ERROR;
	ret = read_msr(cpu, MSR_IA32_APERF, value);
	if (ret < 0) {
		thd_log_warn("get_Aperf_value failed to read\n");
		return THD_ERROR;
	}
	return THD_SUCCESS;
}

