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

/* Parser to parse thermal configuration file. This uses libxml2 API.
 *
 */

#include "thd_parse.h"
#include <stdlib.h>
#include "thd_sys_fs.h"

#define DEBUG_PARSER_PRINT(x,...)

cthd_parse::cthd_parse()
	:matched_thermal_info_index(-1),
	 doc(NULL),
	 root_element(NULL)
{
	std::string name = TDCONFDIR;
	filename=name+"/""thermal-conf.xml";
}

int cthd_parse::parser_init()
{

	doc = xmlReadFile(filename.c_str(), NULL, 0);
	if (doc == NULL) {
		thd_log_warn("error: could not parse file %s\n", filename.c_str());
		return THD_ERROR;
	}
	root_element = xmlDocGetRootElement(doc);

	if (root_element == NULL) {
		thd_log_warn("error: could not get root element \n");
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_trip_point(xmlNode * a_node, xmlDoc *doc, trip_point_t *trip_pt)
{
	xmlNode *cur_node = NULL;
	char *tmp_value;

	trip_pt->cool_dev_id = 0;
	trip_pt->temperature = 0;
	trip_pt->trip_pt_type = ACTIVE;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char *)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char*)cur_node->name, "Temperature")) {
				trip_pt->temperature = atoi(tmp_value);
			}
			if (!strcmp((const char*)cur_node->name, "Hyst")) {
				trip_pt->hyst = atoi(tmp_value);
			}
			if (!strcmp((const char*)cur_node->name, "CoolingDevice")) {
				trip_pt->cool_dev_id = atoi(tmp_value);
			}
			if (!strcmp((const char*)cur_node->name, "type")) {
				if (!strcmp(tmp_value, "active"))
					trip_pt->trip_pt_type = ACTIVE;
				else if (!strcmp(tmp_value, "passive"))
					trip_pt->trip_pt_type = PASSIVE;
				else if (!strcmp(tmp_value, "critical"))
					trip_pt->trip_pt_type = CRITICAL;
				else if (!strcmp(tmp_value, "hot"))
					trip_pt->trip_pt_type = HOT;
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_trip_points(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	trip_point_t trip_pt;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcmp((const char*)cur_node->name, "TripPoint")) {
				trip_pt.cool_dev_id = trip_pt.hyst = trip_pt.temperature = 0;
				trip_pt.trip_pt_type = P_T_STATE;
				if (parse_new_trip_point(cur_node->children, doc, &trip_pt) == THD_SUCCESS)
					info_ptr->trip_pts.push_back(trip_pt);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_pid_values(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char*)cur_node->name, "Kp")) {
				info_ptr->pid.Kp = atof(tmp_value);
			} else if (!strcmp((const char*)cur_node->name, "Kd")) {
				info_ptr->pid.Kd = atof(tmp_value);
			} else if (!strcmp((const char*)cur_node->name, "Ki")) {
				info_ptr->pid.Ki = atof(tmp_value);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_zone(xmlNode * a_node, xmlDoc *doc, thermal_zone_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	char *tmp_value;

	info_ptr->enable_pid = false;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char*)cur_node->name, "TripPoints")) {
				parse_trip_points(cur_node->children, doc, info_ptr);
			}
			else if (!strcmp((const char*)cur_node->name, "Name"))
					info_ptr->name.assign(tmp_value);
			else if (!strcmp((const char*)cur_node->name, "Sensor_path"))
					info_ptr->path.assign(tmp_value);
			else if (!strcmp((const char*)cur_node->name, "Relationship"))
					info_ptr->relationship.assign(tmp_value);
			else if (!strcmp((const char*)cur_node->name, "PidControl")) {
				info_ptr->enable_pid = true;
				parse_pid_values(cur_node->children, doc, info_ptr);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_cooling_dev(xmlNode * a_node, xmlDoc *doc, cooling_dev_t *cdev)
{
	xmlNode *cur_node = NULL;
	char *tmp_value;

	cdev->max_state = cdev->min_state = 0;
	cdev->inc_dec_step = 1 ;
	cdev->auto_down_control = false;
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char *)cur_node->name, "Index")) {
				cdev->index = atoi(tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "Type")) {
				cdev->type_string.assign((const char*)tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "Path")) {
				cdev->path_str.assign((const char*)tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "MinState")) {
				cdev->min_state = atoi(tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "MaxState")) {
				cdev->max_state = atoi(tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "IncDecStep")) {
				cdev->inc_dec_step = atoi(tmp_value);
			} else if (!strcmp((const char *)cur_node->name, "AutoOffMode")) {
				if (atoi(tmp_value))
					cdev->auto_down_control = true;
				else
					cdev->auto_down_control = false;
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_cooling_devs(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	cooling_dev_t cdev;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcmp((const char*)cur_node->name, "CoolingDevice")) {
				cdev.index = cdev.max_state = cdev.min_state = 0;
				cdev.inc_dec_step = 1;
				cdev.auto_down_control = false;
				cdev.path_str.clear();
				cdev.type_string.clear();
				parse_new_cooling_dev(cur_node->children, doc, &cdev);
				info_ptr->cooling_devs.push_back(cdev);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_thermal_zones(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	thermal_zone_t zone;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcmp((const char*)cur_node->name, "ThermalZone")) {
				zone.name.clear();
				zone.path.clear();
				zone.relationship.clear();
				zone.trip_pts.clear();
				parse_new_zone(cur_node->children, doc, &zone);
				info_ptr->zones.push_back(zone);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_platform_info(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char*)cur_node->name, "uuid")) {
				info_ptr->uuid.assign((const char*)tmp_value);
			} else if (!strcmp((const char*)cur_node->name, "Name")) {
				info_ptr->name.assign((const char*)tmp_value);
			} else if (!strcmp((const char*)cur_node->name, "Preference")) {
				if (!strcmp(tmp_value, "PERFORMANCE"))
					info_ptr->default_prefernce = PREF_PERFORMANCE;
				else
					info_ptr->default_prefernce = PREF_ENERGY_CONSERVE;
			} else if (!strcmp((const char*)cur_node->name, "ThermalZones")) {
				parse_thermal_zones(cur_node->children, doc, info_ptr);
			} else if (!strcmp((const char*)cur_node->name, "CoolingDevices")) {
				parse_cooling_devs(cur_node->children, doc, info_ptr);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_platform(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr)
{
	xmlNode *cur_node = NULL;
	unsigned char *tmp_value;
	thermal_info_t info;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (unsigned char*)xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
			if (!strcmp((const char*)cur_node->name, "Platform")) {
				info.cooling_devs.clear();
				info.zones.clear();
				parse_new_platform_info(cur_node->children, doc, &info);
				thermal_info_list.push_back(info);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_thermal_conf(xmlNode * a_node, xmlDoc *doc, thermal_info_t *info_ptr)
{
	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcmp((const char*)cur_node->name, "ThermalConfiguration")) {
				parse_new_platform(cur_node->children, doc, info_ptr);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse(xmlNode * a_node, xmlDoc *doc)
{
	xmlNode *cur_node = NULL;
	thermal_info_t info;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcmp((const char*)cur_node->name, "ThermalConfiguration")) {
				parse_new_platform(cur_node->children, doc, &info);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::start_parse()
{
	parse(root_element, doc);

	return THD_SUCCESS;
}

void cthd_parse::parser_deinit()
{
	xmlFreeDoc(doc);
}

void cthd_parse::dump_thermal_conf()
{
	thd_log_info(" Dumping parsed XML Data\n");
	for (unsigned int i=0; i<thermal_info_list.size(); ++i) {
		thd_log_info(" *** Index %d ***\n", i);
		thd_log_info("Name: %s\n", thermal_info_list[i].name.c_str());
		thd_log_info("UUID: %s\n", thermal_info_list[i].uuid.c_str());
		thd_log_info("type: %d\n", thermal_info_list[i].default_prefernce);
		for (unsigned int j=0; j<thermal_info_list[i].zones.size(); ++j) {
			thd_log_info("\tZone %d \n", j);
			thd_log_info("\t Name: %s\n", thermal_info_list[i].zones[j].name.c_str());
			thd_log_info("\t Relationship: %s\n", thermal_info_list[i].zones[j].relationship.c_str());
			thd_log_info("\t Path: %s\n", thermal_info_list[i].zones[j].path.c_str());
			if (thermal_info_list[i].zones[j].enable_pid) {
				thd_log_info("\t PID: Kp %f\n", thermal_info_list[i].zones[j].pid.Kp);
				thd_log_info("\t PID: Ki %f\n", thermal_info_list[i].zones[j].pid.Ki);
				thd_log_info("\t PID: Kd %f\n", thermal_info_list[i].zones[j].pid.Kd);
			}
			for (unsigned int k=0; k<thermal_info_list[i].zones[j].trip_pts.size(); ++k) {
				thd_log_info("\t\t Trip Point %d \n", k);
				thd_log_info("\t\t  temp id %d \n", thermal_info_list[i].zones[j].trip_pts[k].temperature);
				thd_log_info("\t\t  cooling dev id %d \n", thermal_info_list[i].zones[j].trip_pts[k].cool_dev_id);
				thd_log_info("\t\t  type %d \n", thermal_info_list[i].zones[j].trip_pts[k].trip_pt_type);
				thd_log_info("\t\t  hyst id %d \n", thermal_info_list[i].zones[j].trip_pts[k].hyst);
			}
		}
		for (unsigned int l=0; l<thermal_info_list[i].cooling_devs.size(); ++l) {
			thd_log_info("\tCooling Dev %d \n", l);
			thd_log_info("\t\tName: %s\n", thermal_info_list[i].cooling_devs[l].path_str.c_str());
			thd_log_info("\t\tMin: %d\n", thermal_info_list[i].cooling_devs[l].min_state);
			thd_log_info("\t\tMax: %d\n", thermal_info_list[i].cooling_devs[l].max_state);
			thd_log_info("\t\tStep: %d\n", thermal_info_list[i].cooling_devs[l].inc_dec_step);
			thd_log_info("\t\tAutoDownControl: %d\n", thermal_info_list[i].cooling_devs[l].auto_down_control);
			thd_log_info("\t\ttype: %s\n", thermal_info_list[i].cooling_devs[l].type_string.c_str());
		}
	}
}

bool cthd_parse::platform_matched()
{
	csys_fs thd_sysfs("/sys/class/dmi/id/");

	if (thd_sysfs.exists(std::string("product_uuid"))) {
		thd_log_debug("checking UUID\n");
		std::string str;
		if(thd_sysfs.read("product_uuid", str) < 0)
			return false;
		else {
			thd_log_debug("UUID is %s\n", str.c_str());
			for (unsigned int i=0; i<thermal_info_list.size(); ++i) {
				if (thermal_info_list[i].uuid == str) {
					matched_thermal_info_index = i;
					return true;
				}
			}
			return false;
		}
	}

	return false;
}

int cthd_parse::trip_count(unsigned int zone_index)
{
	if (zone_index < thermal_info_list[matched_thermal_info_index].zones.size()) {
		return thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts.size();
	} else
		return -1;

}

trip_point_t* cthd_parse::get_trip_point(unsigned int zone_index, unsigned int trip_index)
{
	trip_point_t *trip_pt;
	if (zone_index < thermal_info_list[matched_thermal_info_index].zones.size()) {
		if (trip_index < thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts.size())
			return &thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts[trip_index];
		return
				NULL;
	} else
		return NULL;

}

cooling_dev_t* cthd_parse::get_cool_dev_index(unsigned int cdev_index)
{
	cooling_dev_t *cdev;

	if (cdev_index < thermal_info_list[matched_thermal_info_index].cooling_devs.size())
		return &thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index];
	else
		return NULL;
}

bool cthd_parse::pid_status(int zone_index)
{
	return thermal_info_list[matched_thermal_info_index].zones[zone_index].enable_pid;

}

bool cthd_parse::get_pid_values(int zone_index, int *Kp, int *Ki, int *Kd)
{
	if (thermal_info_list[matched_thermal_info_index].zones[zone_index].enable_pid) {
		*Kp = thermal_info_list[matched_thermal_info_index].zones[zone_index].pid.Kp;
		*Kd = thermal_info_list[matched_thermal_info_index].zones[zone_index].pid.Kd;
		*Ki = thermal_info_list[matched_thermal_info_index].zones[zone_index].pid.Ki;
		return true;
	}

	return false;
}

int cthd_parse::set_default_preference()
{
	cthd_preference thd_pref;
	int ret;

	if (thermal_info_list[matched_thermal_info_index].default_prefernce == PREF_PERFORMANCE)
		ret = thd_pref.set_preference("PERFORMANCE");
	else
		ret = thd_pref.set_preference("ENERGY_CONSERVE");

	return ret;
}
