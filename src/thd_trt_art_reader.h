/*
 * thd_trt_art_reader.h: Interface for configuration using ACPI
 * _ART and _TRT tables
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
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

#ifndef THD_TRT_ART_READER_H_
#define THD_TRT_ART_READER_H_

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include <algorithm>

struct rel_object_t {
	std::string target_device;
	std::string target_sensor;
	std::vector<void *> trt_objects;
	std::vector<void *> art_objects;
	std::vector<void *> psvt_objects;
	int temperature;
	int step;

	rel_object_t(std::string name, int _temperature = 0, int _step = 0) {
		target_device = name;
		target_sensor = std::move(name);
		temperature = _temperature;
		step = _step;
	}
};

struct object_finder {
	object_finder(char *key) :
			obj_key(key) {
	}

	bool operator()(const rel_object_t& o) const {
		return obj_key == o.target_device;
	}

	const std::string obj_key;
};

class cthd_acpi_rel {
private:
	std::string rel_cdev;
	std::string xml_hdr;
	std::string conf_begin;
	std::string conf_end;
	std::string output_file_name;

	std::ofstream conf_file;
	std::vector<rel_object_t> rel_list;

	unsigned char *trt_data;
	unsigned int trt_count;

	unsigned char *art_data;
	unsigned int art_count;

	unsigned char *psvt_data;
	unsigned int psvt_count;

	int read_trt();
	void dump_trt();
	void create_platform_conf();
	void create_platform_pref(int perf);
	void create_thermal_zones();
	void create_thermal_zone(std::string type);
	void add_passive_trip_point(rel_object_t &rel_obj);
	void add_psvt_trip_point(rel_object_t &rel_obj);
	void add_active_trip_point(rel_object_t &rel_obj);
	void parse_target_devices();

	int read_art();
	void dump_art();

	int read_psvt();
	int process_psvt(std::string file_name);
	void dump_psvt();

public:
	std::string indentation;

	cthd_acpi_rel();

	int generate_conf(std::string file_name);
};

#endif /* THD_TRT_ART_READER_H_ */
