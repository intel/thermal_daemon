/*
 * libFuzzer target for thermald's GDDV (data vault) parser.
 * Build and run with test/fuzz/run.sh.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "gddv_test.h"

#include <cstdint>
#include <vector>

static void quiet_log(const gchar *, GLogLevelFlags, const gchar *, gpointer)
{
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	/* The parser warns about every malformed field; keep the fuzzer fast */
	g_log_set_default_handler(quiet_log, nullptr);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	std::vector<char> buf(data, data + size);
	std::string json;

	cthd_gddv_test::parse_and_dump(buf.data(), buf.size(), json);
	return 0;
}
