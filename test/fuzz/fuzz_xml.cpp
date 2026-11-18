/*
 * libFuzzer target for thermald's XML configuration parsers:
 * thermal-conf.xml (cthd_parse), thermald-features.xml
 * (cthd_features_parse) and thermal-cpu-cdev-order.xml
 * (cthd_cdev_order_parse). Build and run with test/fuzz/run.sh.
 *
 * Each input is written to all three files and read by the real
 * parser_init(), as thermald does at start, so the file checks, the
 * DTD rejection and libxml2's options are those of the daemon. The
 * parsers only accept root-owned files: run.sh runs this in a user
 * namespace (unshare -r), where the files it writes are owned by uid 0.
 * The build sets TDCONFDIR to a scratch directory (THD_FUZZ_CONFDIR).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "thd_parse.h"
#include "thd_features_parse.h"
#include "thd_cdev_order_parser.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static const std::string conf_dir = TDCONFDIR;
static const std::string files[] = {
	conf_dir + "/thermal-conf.xml",
	conf_dir + "/thermald-features.xml",
	conf_dir + "/thermal-cpu-cdev-order.xml",
};

static void quiet_log(const gchar *, GLogLevelFlags, const gchar *, gpointer)
{
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	/* THD_FUZZ_LOG=1: keep thermald's messages, to check an input */
	if (!getenv("THD_FUZZ_LOG"))
		g_log_set_default_handler(quiet_log, nullptr);
	mkdir(conf_dir.c_str(), 0755);

	/* Fail early instead of fuzzing a parser that rejects every file */
	std::string probe = conf_dir + "/probe";
	int fd = open(probe.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	struct stat st;
	if (fd < 0 || fstat(fd, &st) || st.st_uid != 0) {
		fprintf(stderr, "fuzz_xml: %s must be writable and files created "
				"there owned by uid 0; run in a user namespace "
				"(unshare -r), see test/fuzz/run.sh\n", conf_dir.c_str());
		exit(2);
	}
	close(fd);
	unlink(probe.c_str());
	return 0;
}

static void write_file(const std::string &path, const uint8_t *data,
		size_t size)
{
	int fd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		abort();
	while (size) {
		ssize_t n = write(fd, data, size);
		if (n <= 0)
			abort();
		data += n;
		size -= n;
	}
	close(fd);
}

/* What thermald does with a parsed thermal-conf.xml, short of creating
 * zones and cooling devices (the VM scenarios cover that) */
static void use_thermal_conf(cthd_parse &parser)
{
	parser.dump_thermal_conf();
	if (!parser.platform_matched())
		return;

	parser.get_polling_interval();
	parser.get_ppcc_param("TCPU.D0");
	for (int i = 0; i < parser.sensor_count(); ++i)
		parser.get_sensor_dev_index(i);
	for (int i = 0; i < parser.zone_count(); ++i) {
		parser.get_zone_dev_index(i);
		for (int j = 0; j < parser.trip_count(i); ++j)
			parser.get_trip_point(i, j);
	}
	for (int i = 0; i < parser.cdev_count(); ++i) {
		int kp, ki, kd;

		parser.get_cool_dev_index(i);
		parser.pid_status(i);
		parser.get_pid_values(i, &kp, &ki, &kd);
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	for (const std::string &f : files)
		write_file(f, data, size);

	{
		cthd_parse parser;
		if (parser.parser_init(files[0]) == THD_SUCCESS) {
			parser.start_parse();
			use_thermal_conf(parser);
			parser.parser_deinit();
		}
	}
	{
		cthd_features_parse parser;
		if (parser.parser_init() == THD_SUCCESS) {
			parser.start_parse();
			parser.parser_deinit();
		}
	}
	{
		cthd_cdev_order_parse parser;
		std::vector<std::string> list;
		if (parser.parser_init() == THD_SUCCESS) {
			parser.start_parse();
			parser.get_order_list(list);
			parser.parser_deinit();
		}
	}
	return 0;
}
