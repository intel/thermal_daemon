/*
 * libFuzzer target for thermald's string helpers that see outside
 * input: parse_int_value() and parse_double_value() (XML values and
 * sysfs reads) and is_valid_thermal_object_name() (zone, sensor and
 * cooling device names from D-Bus and XML). Build and run with
 * test/fuzz/run.sh.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "thd_util.h"

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <glib.h>
#include <string>

static void quiet_log(const gchar *, GLogLevelFlags, const gchar *, gpointer)
{
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	/* Every rejected value is logged; keep the fuzzer fast */
	g_log_set_default_handler(quiet_log, nullptr);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	std::string s(reinterpret_cast<const char *>(data), size);
	int i;
	double d;

	if (parse_int_value(s, &i, -1000, 1000) == 0 && (i < -1000 || i > 1000))
		abort();
	parse_int_value(s, &i, INT_MIN, INT_MAX);
	if (parse_double_value(s, &d, -1.0, 1.0) == 0
			&& (!std::isfinite(d) || d < -1.0 || d > 1.0))
		abort();
	parse_double_value(s, &d, -DBL_MAX, DBL_MAX);

	if (is_valid_thermal_object_name(s)
			&& (s.empty() || s.find('/') != std::string::npos
				|| s.find('\0') != std::string::npos))
		abort();

	thd_strcasecmp_n(s.c_str(), "DbusControl");
	return 0;
}
