/*
 * thd_cdev_msr_pstates_limited.h: thermal cooling class interface
 *	using p states, but not full range
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

#ifndef THD_CDEV_MSR_PSTATES_LIMITED_H_
#define THD_CDEV_MSR_PSTATES_LIMITED_H_

#include "thd_cdev_msr_pstates.h"

class cthd_cdev_pstate_msr_limited: public cthd_cdev_pstate_msr
{

public:
	cthd_cdev_pstate_msr_limited(unsigned int _index, int _cpu_index):
		cthd_cdev_pstate_msr(_index, _cpu_index){

	}

	int update()
	{
		highest_freq_state = msr.get_max_turbo_freq();
		lowest_freq_state = msr.get_min_freq();
		thd_log_debug("cthd_cdev_pstate_msr_limited original cpu_index %d min %x max %x\n",
	cpu_index, lowest_freq_state, highest_freq_state);

		max_state = (highest_freq_state - lowest_freq_state)/2;
		lowest_freq_state = lowest_freq_state + max_state;
		thd_log_debug("cthd_cdev_pstate_msr_limited modified cpu_index %d min %x max %x\n",
	cpu_index, lowest_freq_state, highest_freq_state);

		return init();
	}
};





#endif /* THD_CDEV_MSR_PSTATES_LIMITED_H_ */
