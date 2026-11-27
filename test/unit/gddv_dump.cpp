/*
 * gddv_dump: run thermald's GDDV parser on a data vault file and print
 * the parsed tables as JSON on stdout.
 *
 *   gddv_dump <data_vault file>
 *
 * Exit status: 0 parsed, 1 rejected by the parser, 2 usage/IO error.
 * THD_TEST_DEBUG=1 prints the parser's debug messages on stderr.
 * Only the parser runs: no sysfs, D-Bus or input devices. Used by the
 * unit tests (test/unit/test_gddv_parser.py) and as the basis of the
 * fuzz target (test/fuzz/).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "gddv_test.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

extern gboolean log_debug;

int main(int argc, char **argv)
{
	/* THD_TEST_DEBUG=1: print thermald's debug and info messages */
	if (getenv("THD_TEST_DEBUG")) {
		log_debug = TRUE;
		setenv("G_MESSAGES_DEBUG", "all", 1);
	}

	if (argc != 2) {
		fprintf(stderr, "usage: %s <data_vault file>\n", argv[0]);
		return 2;
	}

	std::ifstream f(argv[1], std::ios::binary);
	if (!f) {
		perror(argv[1]);
		return 2;
	}
	std::vector<char> buf((std::istreambuf_iterator<char>(f)),
			std::istreambuf_iterator<char>());

	std::string json;
	int ret = cthd_gddv_test::parse_and_dump(buf.data(), buf.size(), json);
	fputs(json.c_str(), stdout);

	return ret ? 1 : 0;
}
