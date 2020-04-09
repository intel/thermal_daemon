/*
 * thd_trt_art_reader.cpp: Create configuration using ACPI
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
#include <dirent.h>
#include "thd_common.h"
#include "thd_sys_fs.h"

#include "thd_trt_art_reader.h"
#include "acpi_thermal_rel_ioct.h"
#include "thd_int3400.h"

using namespace std;

#define PRINT_ERROR(...)	thd_log_info(__VA_ARGS__)
#define PRINT_DEBUG(...)	thd_log_debug(__VA_ARGS__)

typedef struct {
	const char *source;
	const char *sub_string;
} sub_string_t;


sub_string_t source_substitue_strings[] = { { "TPCH", "pch_wildcat_point" }, {
		NULL, NULL } };

sub_string_t target_substitue_strings[] = { { "B0D4", "rapl_controller" }, {
		"DPLY", "LCD" }, { "DISP", "LCD" }, { "TMEM", "rapl_controller_dram" },
		{ "TCPU", "rapl_controller" }, { "B0DB", "rapl_controller" }, { NULL,
		NULL } };

sub_string_t sensor_substitue_strings[] = { { "TPCH", "pch_wildcat_point" }, {
		NULL, NULL } };

typedef enum {
	TARGET_DEV, SOURCE_DEV, SENSOR_DEV
} sub_type_t;

/*
 * The _TRT and _ART table may refer to entry, for which we have
 * we need to tie to some control device, which is not enumerated
 * as a thermal cooling device. In this case, we substitute them
 * to a inbuilt standard name.
 */
static void associate_device(sub_type_t type, string &name) {
	DIR *dir;
	struct dirent *entry;
	std::string base_path = "/sys/bus/platform/devices/";

	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "INT340", strlen("INT340"))) {
				char buf[256];
				int ret;
				std::string name_path = base_path + entry->d_name
						+ "/firmware_node";
				ret = readlink(name_path.c_str(), buf, sizeof(buf) - 1);
				if (ret > 0) {
					buf[ret] = '\0';
					name_path.clear();
					name_path = base_path + entry->d_name + "/"
							+ std::string(buf) + "/";
					csys_fs acpi_sysfs(name_path.c_str());
					std::string uid;
					if (acpi_sysfs.exists("uid")) {
						ret = acpi_sysfs.read("uid", uid);
						if (ret < 0)
							continue;
					} else if (acpi_sysfs.exists("path")) {
						ret = acpi_sysfs.read("path", uid);
						if (ret < 0)
							continue;
						size_t pos = uid.find_last_of(".");
						if (pos != std::string::npos) {
							uid = uid.substr(pos + 1);
						}
					} else
						continue;
					if (name == uid) {
						if (name_path.find("INT3406") != std::string::npos) {
							name = "DISP";
						} else if (name_path.find("INT3402")
								!= std::string::npos) {
							name = "TMEM";
						} else if (name_path.find("INT3401")
								!= std::string::npos) {
							name = "TCPU";
						}
						closedir(dir);
						return;
					}
				}
			}
		}
		closedir(dir);
	}
}

static void subtitute_string(sub_type_t type, string &name) {
	int i = 0;
	sub_string_t *list;

	if (type == TARGET_DEV) {
		associate_device(type, name);
		list = target_substitue_strings;
	} else if (type == SOURCE_DEV)
		list = source_substitue_strings;
	else
		list = sensor_substitue_strings;

	while (list[i].source) {
		if (name == list[i].source) {
			name = list[i].sub_string;
			break;
		}
		i++;
	}
}

cthd_acpi_rel::cthd_acpi_rel() :
		rel_cdev("/dev/acpi_thermal_rel"), xml_hdr("<?xml version=\"1.0\"?>\n"), conf_begin(
				"<ThermalConfiguration>\n"), conf_end(
				"</ThermalConfiguration>\n"), conf_file(), trt_data(NULL), trt_count(
				0), art_data(NULL), art_count(0) {

}
int cthd_acpi_rel::generate_conf(std::string file_name) {
	int trt_status;
	string prefix;
	int art_status;
	int ret = 0;
	cthd_INT3400 int3400("42A441D6-AE6A-462b-A84B-4A8CE79027D3");

	std::ifstream conf_file_check(file_name.c_str());
	if (conf_file_check.is_open()) {
		PRINT_ERROR(" File Exists: file_name %s, so no regenerate\n",
				file_name.c_str());
		conf_file_check.close();
		return 0;
	}
	conf_file_check.close();

	art_status = read_art();
	trt_status = read_trt();
	if (trt_status < 0 && art_status < 0) {
		PRINT_ERROR("TRT/ART read failed\n");
		return -1;
	}

	if (int3400.match_supported_uuid() != THD_SUCCESS) {
		thd_log_info("Passive 1 UUID is not present, hence ignore _TRT, as it may have junk!!");
		ret = -1;
		goto cleanup;
	}

	conf_file.open(file_name.c_str());
	if (!conf_file.is_open()) {
		PRINT_ERROR("failed to open output file [%s]\n", file_name.c_str());
		ret = -1;
		goto cleanup;
	}
	conf_file << xml_hdr.c_str();
	conf_file << conf_begin.c_str();

	prefix = indentation = "\t";
	conf_file << indentation.c_str() << "<Platform>" << "\n";

	create_platform_conf();
	create_platform_pref(0);
	create_thermal_zones();

	conf_file << prefix.c_str() << "</Platform>" << "\n";

	conf_file << conf_end.c_str();
	conf_file.close();

	cleanup: if (trt_status > 0)
		delete[] trt_data;

	if (art_status > 0)
		delete[] art_data;

	return ret;
}

int cthd_acpi_rel::read_art() {
	int fd;
	int ret;
	unsigned long count, length;

	fd = open(rel_cdev.c_str(), O_RDWR);
	if (fd < 0) {
		PRINT_ERROR("failed to open %s \n", rel_cdev.c_str());
		return -1;
	}

	ret = ioctl(fd, ACPI_THERMAL_GET_ART_COUNT, &count);
	if (ret < 0) {
		PRINT_ERROR(" failed to GET COUNT on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}
	PRINT_DEBUG("ART count %lu ...\n", count);

	ret = ioctl(fd, ACPI_THERMAL_GET_ART_LEN, &length);
	if (ret < 0 || !length) {
		PRINT_ERROR(" failed to GET LEN on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}
	PRINT_DEBUG("ART length %lu ...\n", length);

	art_data = (unsigned char*) new char[length];
	if (!art_data) {
		PRINT_ERROR("cannot allocate buffer %lu to read ART\n", length);
		close(fd);
		return -1;
	}
	ret = ioctl(fd, ACPI_THERMAL_GET_ART, art_data);
	if (ret < 0) {
		PRINT_ERROR(" failed to GET ART on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}
	art_count = count;
	dump_art();

	close(fd);

	return 0;
}

int cthd_acpi_rel::read_trt() {
	int fd;
	int ret;
	unsigned long count, length;

	fd = open(rel_cdev.c_str(), O_RDWR);
	if (fd < 0) {
		PRINT_ERROR("failed to open %s \n", rel_cdev.c_str());
		return -1;
	}

	ret = ioctl(fd, ACPI_THERMAL_GET_TRT_COUNT, &count);
	if (ret < 0) {
		PRINT_ERROR(" failed to GET COUNT on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}
	PRINT_DEBUG("TRT count %lu ...\n", count);

	ret = ioctl(fd, ACPI_THERMAL_GET_TRT_LEN, &length);
	if (ret < 0 || !length) {
		PRINT_ERROR(" failed to GET LEN on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}

	trt_data = (unsigned char*) new char[length];
	if (!trt_data) {
		PRINT_ERROR("cannot allocate buffer %lu to read TRT\n", length);
		close(fd);
		return -1;
	}
	ret = ioctl(fd, ACPI_THERMAL_GET_TRT, trt_data);
	if (ret < 0) {
		PRINT_ERROR(" failed to GET TRT on %s\n", rel_cdev.c_str());
		close(fd);
		return -1;
	}
	trt_count = count;
	dump_trt();

	close(fd);

	return 0;
}

void cthd_acpi_rel::add_passive_trip_point(rel_object_t &rel_obj) {
	if (!rel_obj.trt_objects.size())
		return;

	string prefix = indentation + "\t";

	conf_file << prefix.c_str() << "<TripPoint>\n";

	subtitute_string(SENSOR_DEV, rel_obj.target_sensor);

	conf_file << prefix.c_str() << "\t" << "<SensorType>"
			<< rel_obj.target_sensor.c_str() << "</SensorType>\n";

	conf_file << prefix.c_str() << "\t" << "<Temperature>" << "*"
			<< "</Temperature>\n";

	conf_file << prefix.c_str() << "\t" << "<type>" << "passive" "</type>\n";

	conf_file << prefix.c_str() << "\t" << "<ControlType>" << "SEQUENTIAL"
			<< "</ControlType>\n";

	for (unsigned int j = 0; j < rel_obj.trt_objects.size(); ++j) {
		union trt_object *object = (union trt_object *) rel_obj.trt_objects[j];
		string device_name = object->acpi_trt_entry.source_device;

		conf_file << prefix.c_str() << "\t" << "<CoolingDevice>\n";

		subtitute_string(TARGET_DEV, device_name);

		conf_file << prefix.c_str() << "\t\t" << "<type>" << device_name.c_str()
				<< "</type>\n";
		conf_file << prefix.c_str() << "\t\t" << "<influence>"
				<< object->acpi_trt_entry.influence << "</influence>\n";
		conf_file << prefix.c_str() << "\t\t" << "<SamplingPeriod>"
				<< object->acpi_trt_entry.sample_period * 100 / 1000
				<< "</SamplingPeriod>\n";

		conf_file << prefix.c_str() << "\t" << "</CoolingDevice>\n";
	}
	conf_file << prefix.c_str() << "</TripPoint>\n";
}

void cthd_acpi_rel::add_active_trip_point(rel_object_t &rel_obj) {
	if (!rel_obj.art_objects.size())
		return;

	string prefix = indentation + "\t";

	conf_file << prefix.c_str() << "<TripPoint>\n";

	subtitute_string(SENSOR_DEV, rel_obj.target_sensor);

	conf_file << prefix.c_str() << "\t" << "<SensorType>"
			<< rel_obj.target_sensor.c_str() << "</SensorType>\n";

	conf_file << prefix.c_str() << "\t" << "<Temperature>" << "*"
			<< "</Temperature>\n";

	conf_file << prefix.c_str() << "\t" << "<type>" << "active" "</type>\n";

	conf_file << prefix.c_str() << "\t" << "<ControlType>" << "SEQUENTIAL"
			<< "</ControlType>\n";

	for (unsigned int j = 0; j < rel_obj.art_objects.size(); ++j) {
		union art_object *object = (union art_object *) rel_obj.art_objects[j];
		string device_name = object->acpi_art_entry.source_device;

		conf_file << prefix.c_str() << "\t" << "<CoolingDevice>\n";

		subtitute_string(TARGET_DEV, device_name);
		conf_file << prefix.c_str() << "\t\t" << "<type>" << device_name.c_str()
				<< "</type>\n";
		conf_file << prefix.c_str() << "\t\t" << "<influence>"
				<< object->acpi_art_entry.weight << "</influence>\n";

		conf_file << prefix.c_str() << "\t" << "</CoolingDevice>\n";
	}
	conf_file << prefix.c_str() << "</TripPoint>\n";
}

void cthd_acpi_rel::create_thermal_zone(string type) {
	unsigned int i;

	indentation += "\t";
	string prefix = indentation;
	indentation += '\t';
	parse_target_devices();
	for (i = 0; i < rel_list.size(); ++i) {
		if (rel_list[i].target_device == "") {
			PRINT_ERROR("Empty target, skipping ..\n");
			continue;
		}
		conf_file << prefix.c_str() << "<ThermalZone>" << "\n";

		subtitute_string(SOURCE_DEV, rel_list[i].target_device);
		conf_file << prefix.c_str() << "\t" << "<Type>"
				<< rel_list[i].target_device.c_str() << "</Type>" << "\n";
		conf_file << prefix.c_str() << "\t" << "<TripPoints>" << "\n";
		indentation += "\t";
		add_passive_trip_point(rel_list[i]);
		add_active_trip_point(rel_list[i]);
		conf_file << prefix.c_str() << "\t" << "</TripPoints>" << "\n";

		conf_file << prefix.c_str() << "</ThermalZone>" << "\n";
	}
}

void cthd_acpi_rel::parse_target_devices() {
	union trt_object *trt = (union trt_object *) trt_data;
	union art_object *art = (union art_object *) art_data;

	unsigned int i;

	for (i = 0; i < trt_count; i++) {
		rel_object_t rel_obj(trt[i].acpi_trt_entry.target_device);

		vector<rel_object_t>::iterator find_iter;

		find_iter = find_if(rel_list.begin(), rel_list.end(),
				object_finder(trt[i].acpi_trt_entry.target_device));

		if (find_iter == rel_list.end()) {
			rel_obj.trt_objects.push_back(&trt[i]);
			rel_list.push_back(rel_obj);
		} else
			find_iter->trt_objects.push_back(&trt[i]);
	}

	for (i = 0; i < art_count; i++) {
		rel_object_t rel_obj(art[i].acpi_art_entry.target_device);

		vector<rel_object_t>::iterator find_iter;

		find_iter = find_if(rel_list.begin(), rel_list.end(),
				object_finder(art[i].acpi_art_entry.target_device));

		if (find_iter == rel_list.end()) {
			rel_obj.art_objects.push_back(&art[i]);
			rel_list.push_back(rel_obj);
		} else
			find_iter->art_objects.push_back(&art[i]);
	}
}

void cthd_acpi_rel::create_thermal_zones() {
	string prefix = indentation;
	conf_file << prefix.c_str() << "<ThermalZones>" << "\n";

	// Read ...
	create_thermal_zone("test");
	//
	conf_file << prefix.c_str() << "</ThermalZones>" << "\n";

}

void cthd_acpi_rel::create_platform_conf() {
//	string line;
	string prefix;

	indentation += "\t";

	prefix = indentation;
	conf_file << prefix.c_str() << "<Name>" << "_TRT export" << "</Name>"
			<< "\n";

	ifstream product_name("/sys/class/dmi/id/product_name");

	conf_file << indentation.c_str() << "<ProductName>";
#if 0
	if (product_name.is_open() && getline(product_name, line)) {
#else
	char buffer[256];
	if (product_name.is_open()
			&& product_name.getline(buffer, sizeof(buffer))) {
		string line(buffer);
#endif
		conf_file << line.c_str();
	} else
		conf_file << "*" << "\n";

	conf_file << prefix.c_str() << "</ProductName>" << "\n";
}

#if LOG_DEBUG_INFO == 1
void cthd_acpi_rel::dump_trt() {

	union trt_object *trt = (union trt_object *) trt_data;
	unsigned int i;

	for (i = 0; i < trt_count; i++) {
		PRINT_DEBUG("TRT %d: SRC %s:\t", i,
				trt[i].acpi_trt_entry.source_device);
		PRINT_DEBUG("TRT %d: TGT %s:\t", i,
				trt[i].acpi_trt_entry.target_device);
		PRINT_DEBUG("TRT %d: INF %llu:\t", i, trt[i].acpi_trt_entry.influence);
		PRINT_DEBUG("TRT %d: SMPL %llu:\n", i,
				trt[i].acpi_trt_entry.sample_period);
	}
}

void cthd_acpi_rel::dump_art() {

	union art_object *art = (union art_object *) art_data;
	unsigned int i;

	for (i = 0; i < art_count; i++) {
		PRINT_DEBUG("ART %d: SRC %s:\t", i,
				art[i].acpi_art_entry.source_device);
		PRINT_DEBUG("ART %d: TGT %s:\t", i,
				art[i].acpi_art_entry.target_device);
		PRINT_DEBUG("ART %d: WT %llu:\n", i, art[i].acpi_art_entry.weight);
	}
}
#else
void cthd_acpi_rel::dump_trt() {
}

void cthd_acpi_rel::dump_art() {
}
#endif

void cthd_acpi_rel::create_platform_pref(int perf) {
	if (perf)
		conf_file << indentation.c_str()
				<< "<Preference>PERFORMANCE</Preference>" << "\n";
	else
		conf_file << indentation.c_str() << "<Preference>QUIET</Preference>"
				<< "\n";
}
