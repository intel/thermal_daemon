/*
 * thd_util.cpp: Common utility functions
 *
 * Copyright (C) 2026 Intel Corporation. All rights reserved.
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

#include "thd_util.h"

bool starts_with(const std::string& s, const char *prefix)
{
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, len, prefix) == 0;
}


static const size_t THD_MAX_STR_CMP_LEN = 4096;

size_t thd_cmp_len(const char *param1, const char *param2) {
	size_t param1_len;
	size_t param2_len;

	if (!param1)
		param1 = "";
	if (!param2)
		param2 = "";

	param1_len = strnlen(param1, THD_MAX_STR_CMP_LEN);
	param2_len = strnlen(param2, THD_MAX_STR_CMP_LEN);

	return (param1_len > param2_len) ? param1_len : param2_len;
}

int thd_strcmp_n(const char *param1, const char *param2) {
	if (!param1)
		param1 = "";
	if (!param2)
		param2 = "";

	return strncmp(param1, param2, thd_cmp_len(param1, param2));
}

int thd_strcasecmp_n(const char *param1, const char *param2) {
	if (!param1)
		param1 = "";
	if (!param2)
		param2 = "";

	return strncasecmp(param1, param2, thd_cmp_len(param1, param2));
}
