/*
 * thd_int3400.h: Load and check INT3400 uuids for match
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

#ifndef SRC_THD_INT3400_UUID_H_
#define SRC_THD_INT3400_UUID_H_

#include "thd_common.h"
#include <string>
#include <vector>

class cthd_INT3400 {
private:
	std::string uuid;
	std::string base_path;
	int set_policy_osc(void);
public:
	cthd_INT3400(std::string _uuid);
	int match_supported_uuid(void);
	void set_default_uuid(void);
};

#endif /* SRC_THD_INT3400_UUID_H_ */
