/*
 * thd_engine.cpp: thermal engine class implementation
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
#ifndef THD_PARSE_H
#define THD_PARSE_H

#include <string>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "thermald.h"
#include "thd_trip_point.h"

#define CDEV_DEF_BIT_MIN_STATE	0x0001
#define CDEV_DEF_BIT_MAX_STATE	0x0002
#define CDEV_DEF_BIT_STEP		0x0004
#define CDEV_DEF_BIT_READ_BACK	0x0008
#define CDEV_DEF_BIT_AUTO_DOWN	0x0010
#define CDEV_DEF_BIT_PATH		0x0020
#define CDEV_DEF_BIT_STATUS		0x0040
#define CDEV_DEF_BIT_UNIT_VAL	0x0080
#define CDEV_DEF_BIT_DEBOUNCE_VAL	0x0100
#define CDEV_DEF_BIT_PID_PARAMS	0x0200
#define CDEV_DEF_BIT_WRITE_PREFIX	0x0400

#define SENSOR_DEF_BIT_PATH		0x0001
#define SENSOR_DEF_BIT_ASYNC_CAPABLE		0x0002

typedef struct {
	double Kp;
	double Ki;
	double Kd;
} pid_control_t;

typedef struct {
	std::string name;
	double multiplier;
	double offset;
} thermal_sensor_link_t;

typedef struct {
	unsigned int mask;
	std::string name;
	std::string path;
	bool async_capable;
	bool virtual_sensor;
	thermal_sensor_link_t sensor_link;
} thermal_sensor_t;

typedef struct {
	int dependency;
	std::string cdev;
	std::string state;
}trip_cdev_depend_t;

typedef struct {
	std::string type;
	int influence;
	int sampling_period;
	int target_state_valid;
	int target_state;
	pid_param_t pid_param;
} trip_cdev_t;

typedef struct {
	int temperature;
	int hyst;
	trip_point_type_t trip_pt_type;
	trip_control_type_t control_type;
	int influence;
	std::string sensor_type;
	trip_cdev_depend_t dependency;
	std::vector<trip_cdev_t> cdev_trips;
} trip_point_t;

typedef struct {
	std::string type;
	std::vector<trip_point_t> trip_pts;
} thermal_zone_t;

typedef enum {
	ABSOULUTE_VALUE, RELATIVE_PERCENTAGES
} unit_value_t;

typedef struct {
	bool status;
	unsigned int mask; // Fields which are present in config
	int index;
	unit_value_t unit_val;
	int min_state;
	int max_state;
	int inc_dec_step;
	bool read_back; // For some device read back current state is not possible
	bool auto_down_control;
	std::string type_string;
	std::string path_str;
	int debounce_interval;
	bool pid_enable;
	pid_control_t pid;
	std::string write_prefix;
} cooling_dev_t;

typedef struct {
	std::string name;
	int valid;
	int power_limit_min;
	int power_limit_max;
	int time_wind_min;
	int time_wind_max;
	int step_size;
} ppcc_t;

typedef struct {
	std::string name;
	std::string uuid;
	std::string product_name;
	std::string product_sku;
	int default_preference;
	int polling_interval;
	ppcc_t ppcc;
	std::vector<thermal_sensor_t> sensors;
	std::vector<thermal_zone_t> zones;
	std::vector<cooling_dev_t> cooling_devs;
} thermal_info_t;

class cthd_parse {
private:
	std::string filename;
	std::string filename_auto;
	std::string filename_auto_conf;
	std::vector<thermal_info_t> thermal_info_list;
	int matched_thermal_info_index;
	xmlDoc *doc;
	xmlNode *root_element;
	int auto_config;

	int parse(xmlNode * a_node, xmlDoc *doc);
	int parse_pid_values(xmlNode * a_node, xmlDoc *doc, pid_control_t *pid_ptr);
	int parse_dependency_values(xmlNode * a_node, xmlDoc *doc, trip_cdev_depend_t *dependency);
	int parse_new_trip_cdev(xmlNode * a_node, xmlDoc *doc,
			trip_cdev_t *trip_cdev);

	int parse_new_thermal_conf(xmlNode * a_node, xmlDoc *doc,
			thermal_info_t *info);
	int parse_new_platform_info(xmlNode * a_node, xmlDoc *doc,
			thermal_info_t *info);
	int parse_new_zone(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr);
	int parse_new_cooling_dev(xmlNode * a_node, xmlDoc *doc,
			cooling_dev_t *info_ptr);
	int parse_new_trip_point(xmlNode * a_node, xmlDoc *doc,
			trip_point_t *trip_pt);
	int parse_thermal_zones(xmlNode * a_node, xmlDoc *doc,
			thermal_info_t *info_ptr);
	int parse_new_sensor(xmlNode * a_node, xmlDoc *doc,
			thermal_sensor_t *info_ptr);
	int parse_new_sensor_link(xmlNode * a_node, xmlDoc *doc,
			thermal_sensor_link_t *info_ptr);
	int parse_thermal_sensors(xmlNode * a_node, xmlDoc *doc,
			thermal_info_t *info_ptr);
	int parse_cooling_devs(xmlNode * a_node, xmlDoc *doc,
			thermal_info_t *info_ptr);
	int parse_trip_points(xmlNode * a_node, xmlDoc *doc,
			thermal_zone_t *info_ptr);
	int parse_new_platform(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info);

	int parse_ppcc(xmlNode * a_node, xmlDoc *doc, ppcc_t *ppcc);

	void string_trim(std::string &str);
	char *char_trim(char *trim);
	bool match_product_sku(int index);

public:
	cthd_parse();
	int parser_init(std::string config_file);
	void parser_deinit();
	int start_parse();
	void dump_thermal_conf();
	bool platform_matched();
	int get_polling_interval();
	ppcc_t *get_ppcc_param(std::string name);
	int zone_count() {
		return thermal_info_list[matched_thermal_info_index].zones.size();
	}
	int cdev_count() {
		return thermal_info_list[matched_thermal_info_index].cooling_devs.size();
	}

	int sensor_count() {
		return thermal_info_list[matched_thermal_info_index].sensors.size();
	}

	int thermal_conf_auto() {
		return auto_config;
	}

	int thermal_matched_platform_index() {
		return matched_thermal_info_index;
	}

	int set_default_preference();
	int trip_count(unsigned int zone_index);
	bool pid_status(int cdev_index);
	bool get_pid_values(int cdev_index, int *Kp, int *Ki, int *Kd);
	trip_point_t *get_trip_point(unsigned int zone_index,
			unsigned int trip_index);
	cooling_dev_t *get_cool_dev_index(unsigned int cdev_index);
	thermal_sensor_t *get_sensor_dev_index(unsigned int sensor_index);
	thermal_zone_t *get_zone_dev_index(unsigned int zone_index);
//	std::string get_sensor_path(int zone_index) {
//		return thermal_info_list[matched_thermal_info_index].zones[zone_index].path;
//	}
};

#endif
