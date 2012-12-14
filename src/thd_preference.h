/*
 * thd_preference.h: Thermal preference class interface file
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

#ifndef THD_PREFERENCE_H
#define THD_PREFERENCE_H

#include "thermald.h"
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>

enum {
	PREF_PERFORMANCE          = 0,
	PREF_ENERGY_CONSERVE,
	PREF_DISABLED,
	PREF_INVALID
};

class cthd_preference {
private:
	int preference;
	int string_pref_to_int(std::string& pref_str);
	std::string int_pref_to_string(int pref);
public:
	cthd_preference();
	bool set_preference(const char *pref);
	std::string get_preference_str();
	const char* get_preference_cstr();
	int get_preference();
};

#endif
