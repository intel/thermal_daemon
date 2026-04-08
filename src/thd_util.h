/*
 * thd_sensor.h: thermal sensor class interface
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
 * Author Name Andrew Gaul <andrew@gaul.org>
 *
 */

#ifndef THD_UTIL_H_
#define THD_UTIL_H_

#include <string>
#include <cstring>

// Replacement for C++20 std::string::starts_with
static bool starts_with(const std::string& s, const char *prefix)__attribute__((unused));

static bool starts_with(const std::string& s, const char *prefix) {
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, s.size(), prefix);
}

#endif /* THD_UTIL_H_ */
