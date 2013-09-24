/*
 * src/thd_cdev_intel_pstate_turbo.h.h: thermal cooling class implementation
 *	using Turbo states using Intel P state driver
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
#ifndef THD_CDEV_INTEL_PSTATE_TURBO_STATES_H_
#define THD_CDEV_INTEL_PSTATE_TURBO_STATES_H_

#include "thd_cdev.h"
#include "thd_msr.h"

class cthd_cdev_intel_pstate_turbo: public cthd_cdev
{
private:

public:
	static const int turbo_states_cnt = 1;
	cthd_cdev_intel_pstate_turbo(unsigned int _index, std::string control_path)
			: cthd_cdev(_index, control_path) {}
	void set_curr_state(int state, int arg);
	int get_max_state();
	int update();
};

#endif /* THD_CDEV_TURBO_STATES_H_ */
