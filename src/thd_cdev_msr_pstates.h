/*
 * thd_cdev_pstates.h: thermal cooling class interface
 *	using T states.
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
#ifndef THD_CDEV_PSTATE_MSR_H_
#define THD_CDEV_PSTATE_MSR_H_

#include "thd_cdev.h"
#include "thd_msr.h"

class cthd_cdev_pstate_msr: public cthd_cdev
{
protected:
	cthd_msr msr;
	int cpu_start_index;
	int cpu_end_index;
	std::string last_governor;
	int highest_freq_state;
	int lowest_freq_state;
	int control_begin();
	int control_end();
	int cpu_index;
	int max_state;
public:
	cthd_cdev_pstate_msr(unsigned int _index, int _cpu_index): cthd_cdev(_index, 
	"/sys/devices/system/cpu/"), cpu_index(_cpu_index), max_state(0){}
	int init();
	void set_curr_state(int state, int arg);
	int get_max_state();
	virtual int update();
};

#endif /* THD_CDEV_TSTATES_H_ */
