/*
 * thd_preference.cpp: Thermal preference class implementation
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

#include "thd_preference.h"

cthd_preference::cthd_preference()
{
	std::stringstream filename;
	filename << TDCONFDIR << "/" << "thd_preference.conf";
        std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
        if (!ifs.good()){
		preference = PREF_PERFORMANCE;
        } else {
		//ifs.read(reinterpret_cast < char * > (&preference), sizeof(preference));
		ifs >> preference;
	}
        ifs.close();
}

std::string cthd_preference::int_pref_to_string(int pref)
{
	std::string perf_str;

	switch (preference) {
		case PREF_PERFORMANCE:
			perf_str = "PERFORMANCE";
			break;
		case PREF_ENERGY_CONSERVE:
			perf_str = "ENERGY_CONSERVE";
			break;
		case PREF_DISABLED:
			perf_str = "DISABLE";
			break;
		default:
			perf_str = "INVALID";
	}

	return perf_str;
}

int cthd_preference::string_pref_to_int(std::string& pref_str)
{
	int pref;
	if (pref_str ==  "PERFORMANCE")
		pref = PREF_PERFORMANCE;
	else if (pref_str == "ENERGY_CONSERVE")
		pref = PREF_ENERGY_CONSERVE;
	else if (pref_str == "DISABLE")
		pref = PREF_DISABLED;
	else
		pref = PREF_INVALID;

	return pref;
}

std::string cthd_preference::get_preference_str()
{
	return int_pref_to_string(preference);
}

const char* cthd_preference::get_preference_cstr()
{
	return int_pref_to_string(preference).c_str();
}

int cthd_preference::get_preference()
{
	return preference;
}

bool cthd_preference::set_preference(const char *pref_str)
{
	std::string str(pref_str);
	int pref = string_pref_to_int(str);
	
	if (pref == PREF_INVALID) {
		return FALSE; 
	}
	std::stringstream filename;
	filename << TDCONFDIR << "/" << "thd_preference.conf";
	std::ofstream fout(filename.str().c_str());
        if (!fout.good()){
		return FALSE;
	}
	fout << pref;
	fout.close();

	return TRUE;
}
