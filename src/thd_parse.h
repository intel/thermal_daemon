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

typedef struct {
	int temperature;
	int hyst;
	trip_point_type_t trip_pt_type;
	int cool_dev_id;
}trip_point_t;

typedef struct {
	float Kp;
	float Ki;
	float Kd;
}pid_control_t;

typedef struct {
	std::string name;
	std::string path;
	std::string relationship;
	std::vector <trip_point_t> trip_pts;
	bool		enable_pid;
	pid_control_t pid;
}thermal_zone_t;

typedef struct {
	int index;
	int min_state;
	int max_state;
	int inc_dec_step;
	bool auto_down_control;
	std::string type_string;
	std::string path_str;
}cooling_dev_t;

typedef struct {
	std::string name;
	std::string uuid;
	int default_prefernce;
	std::vector <thermal_zone_t> zones;
	std::vector <cooling_dev_t> cooling_devs;
}thermal_info_t;

class cthd_parse {
private:
	xmlDoc		*doc;
	xmlNode		*root_element;
	std::string	filename;
	std::vector <thermal_info_t> thermal_info_list;
	int matched_thermal_info_index;

	int parse(xmlNode * a_node, xmlDoc *doc);
	int parse_pid_values(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr);
	int parse_new_thermal_conf(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info);
	int parse_new_platform_info(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info);
	int parse_new_zone(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr);
	int parse_new_cooling_dev(xmlNode * a_node, xmlDoc *doc, cooling_dev_t *info_ptr);
	int parse_new_trip_point(xmlNode * a_node, xmlDoc *doc, trip_point_t *trip_pt);
	int parse_thermal_zones(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr);
	int parse_cooling_devs(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr);
	int parse_trip_points(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr);
	int parse_new_platform(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info);

public:
	cthd_parse();
	int parser_init();
	void parser_deinit();
	int start_parse();
	void dump_thermal_conf();
	bool platform_matched();
	int zone_count() { return thermal_info_list[matched_thermal_info_index].zones.size();}
	int cdev_count() { return thermal_info_list[matched_thermal_info_index].cooling_devs.size();}

	int set_default_preference();
	int trip_count(int zone_index);
	bool pid_status(int zone_index);
	bool get_pid_values(int zone_index, int *Kp, int *Ki, int *Kd);
	trip_point_t *get_trip_point(int zone_index, int trip_index);
	cooling_dev_t *get_cool_dev_index(int cdev_index);
	std::string get_sensor_path(int zone_index) {return thermal_info_list[matched_thermal_info_index].zones[zone_index].path;}
};

#endif
