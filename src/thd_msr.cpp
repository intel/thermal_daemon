/* thd_msr.cpp: thermal engine msr class implementation
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

#include "thd_msr.h"

// MSR defines used in this module
#define MSR_IA32_MISC_ENABLE	0x000001a0
#define MSR_IA32_MISC_ENABLE_TURBO_DISABLE      (1ULL << 38)

#define MSR_IA32_THERM_CONTROL          		0x0000019a

#define MSR_IA32_CLK_MOD_ENABLE					(1ULL << 4)
#define MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK		(0xF)

#define MSR_IA32_PERF_CTL						0x0199
#define	TURBO_DISENGAGE_BIT						(1ULL << 32)

cthd_msr::cthd_msr()
	: msr_sysfs("/dev/cpu/")
{
}	

int cthd_msr::read_msr(int cpu, unsigned int idx, unsigned long long *val)
{
	int ret = -1;
	std::stringstream file_name_str;

	file_name_str << cpu << "/msr";
	if (msr_sysfs.exists(file_name_str.str())) {
		ret = msr_sysfs.read(file_name_str.str(), idx, (char *)val, sizeof(*val));
	}

	return ret;
}

int cthd_msr::write_msr(int cpu, unsigned int idx, unsigned long long val)
{
	int ret = -1;
	std::stringstream file_name_str;
	file_name_str << cpu << "/msr";
	if (msr_sysfs.exists(file_name_str.str())) {
		ret = msr_sysfs.write(file_name_str.str(), idx, val);
	}

	return ret;
}

int cthd_msr::get_no_cpus()
{
	int count = 0;

	for(int i=0; i<64; ++i) {
		std::stringstream file_name_str;

		file_name_str << i;
		if (msr_sysfs.exists(file_name_str.str())) {
			count++;
		}
	}

	return count;
}

bool cthd_msr::check_turbo_status()
{
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	// Return false even if one of the core is not enabled
	for(int i=0; i<cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_MISC_ENABLE, &val);
		if (ret < 0)
			return false;

		if (val & MSR_IA32_MISC_ENABLE_TURBO_DISABLE)
			return false;
	}

	return true;
}

int cthd_msr::enable_turbo()
{
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	for(int i=0; i<cpu_count; ++i) {
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

int cthd_msr::disable_turbo()
{
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	for(int i=0; i<cpu_count; ++i) {
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

int cthd_msr::set_clock_mod_duty_cycle(int state)
{
	int cpu_count = get_no_cpus();
	unsigned long long val;
	int ret;

	thd_log_info("Set T stated %d \n", state);
	// First bit is reserved
	state = state << 1;

	for(int i=0; i<cpu_count; ++i) {
		ret = read_msr(i, MSR_IA32_THERM_CONTROL, &val);
		thd_log_debug("set_clock_mod_duty_cycle current %x\n", val);
		if (ret < 0)
			return THD_ERROR;

		if (!state) {
			val &= ~MSR_IA32_CLK_MOD_ENABLE;
		} else {
			val |= MSR_IA32_CLK_MOD_ENABLE;
		}

		val &= ~MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK;
		val |= (state & MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK);

		thd_log_debug("set_clock_mod_duty_cycle current set to %x\n", val);
		ret = write_msr(i, MSR_IA32_THERM_CONTROL, val);
		if (ret < 0) {
			thd_log_warn("set_clock_mod_duty_cycle current set failed to write\n");
			return THD_ERROR;
		}
	}

	return THD_SUCCESS;
}

int cthd_msr::get_clock_mod_duty_cycle()
{
	int ret, state;
	unsigned long long val;

	// Just get for cpu 0 and return
	ret = read_msr(0, MSR_IA32_THERM_CONTROL, &val);
	thd_log_debug("get_clock_mod_duty_cycle current %x\n", val);
	if (ret < 0)
		return THD_ERROR;

	if (val & MSR_IA32_CLK_MOD_ENABLE) {
		state = val & MSR_IA32_CLK_MOD_DUTY_CYCLE_MASK;
		state = state >> 1;
		thd_log_debug("current state  %x\n", state);
	}

	return state;
}
