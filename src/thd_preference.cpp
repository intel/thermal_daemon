/*
 * thd_preference.cpp: Thermal preference class implementation
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

#include "thd_preference.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

cthd_preference::cthd_preference() :
		preference(PREF_ENERGY_CONSERVE), old_preference(0) {
	std::ostringstream filename;
	filename << TDRUNDIR << "/" << "thd_preference.conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (!ifs.good()) {
		preference = PREF_ENERGY_CONSERVE;
	} else {
		ifs >> preference;
	}
	ifs.close();
}

std::string cthd_preference::int_pref_to_string(int pref) {
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
		break;
	}

	return perf_str;
}

int cthd_preference::string_pref_to_int(std::string &pref_str) {
	int pref;

	if (pref_str == "PERFORMANCE")
		pref = PREF_PERFORMANCE;
	else if (pref_str == "ENERGY_CONSERVE")
		pref = PREF_ENERGY_CONSERVE;
	else if (pref_str == "DISABLE")
		pref = PREF_DISABLED;
	else
		pref = -1;

	return pref;
}

std::string cthd_preference::get_preference_str() {
	return int_pref_to_string(preference);
}

const char *cthd_preference::get_preference_cstr() {
	return strdup(int_pref_to_string(preference).c_str());
}

int cthd_preference::get_preference() {
	return preference;
}

void cthd_preference::refresh() {
	std::ostringstream filename;
	filename << TDRUNDIR << "/" << "thd_preference.conf";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (!ifs.good()) {
		preference = PREF_ENERGY_CONSERVE;
	} else {
		ifs >> preference;
	}
	ifs.close();
}

bool cthd_preference::set_preference(const char *pref_str) {
	std::string str(pref_str);
	int pref = string_pref_to_int(str);

	/* Reject unknown preference values rather than silently coercing
	 * them, so a misbehaving caller cannot blindly switch the daemon
	 * into PERFORMANCE mode. */
	if (pref < 0)
		return false;

	std::ostringstream filename;
	filename << TDRUNDIR << "/" << "thd_preference.conf";

	/* Open with O_NOFOLLOW so that a symlink planted in TDRUNDIR cannot
	 * redirect the write to an arbitrary file. O_TRUNC ensures we don't
	 * leave stale bytes from a longer prior value. */
	int fd = ::open(filename.str().c_str(),
			O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
	if (fd < 0)
		return false;

	std::ostringstream pref_buf;
	pref_buf << pref;
	const std::string &pref_data = pref_buf.str();
	ssize_t w = ::write(fd, pref_data.c_str(), pref_data.size());
	::close(fd);
	if (w != (ssize_t) pref_data.size())
		return false;

	// Save the old preference
	old_preference = preference;
	std::ostringstream filename_save;
	filename_save << TDRUNDIR << "/" << "thd_preference.conf.save";

	int fd_save = ::open(filename_save.str().c_str(),
			O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
	if (fd_save < 0)
		return false;

	std::ostringstream save_buf;
	save_buf << old_preference;
	const std::string &save_data = save_buf.str();
	ssize_t w_save = ::write(fd_save, save_data.c_str(), save_data.size());
	::close(fd_save);
	if (w_save != (ssize_t) save_data.size())
		return false;

	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (!ifs.good()) {
		preference = PREF_PERFORMANCE;
	} else {
		//ifs.read(reinterpret_cast < char * > (&preference), sizeof(preference));
		ifs >> preference;
	}
	ifs.close();

	thd_log_debug("old_preference %d new preference %d\n", old_preference,
			preference);

	return true;
}

int cthd_preference::get_old_preference() {
	std::ostringstream filename;

	filename << TDRUNDIR << "/" << "thd_preference.conf.save";
	std::ifstream ifs(filename.str().c_str(), std::ifstream::in);
	if (!ifs.good()) {
		old_preference = PREF_PERFORMANCE;
	} else {
		//ifs.read(reinterpret_cast < char * > (&preference), sizeof(preference));
		ifs >> old_preference;
	}
	ifs.close();

	return old_preference;
}
