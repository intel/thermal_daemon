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
#include <algorithm>
#include "thd_sys_fs.h"
#include "thd_trt_art_reader.h"

#define DEBUG_PARSER_PRINT(x,...)

void cthd_parse::string_trim(std::string &str) {
	std::string chars = "\n \t\r";

	for (unsigned int i = 0; i < chars.length(); ++i) {
		str.erase(std::remove(str.begin(), str.end(), chars[i]), str.end());
	}
}

char *cthd_parse::char_trim(char *str) {
	int i;

	if (!str)
		return NULL;
	if (str[0] == '\0')
		return str;
	while (isspace(*str))
		str++;
	for (i = strlen(str) - 1; (isspace(str[i])); i--)
		;
	str[i + 1] = '\0';

	return str;
}

cthd_parse::cthd_parse() :
		matched_thermal_info_index(-1), doc(NULL), root_element(NULL), auto_config(
				0) {
	std::string name_conf = TDCONFDIR;
	std::string name_run = TDRUNDIR;
#ifdef ANDROID
	filename = name_run + "/" + "thermal-conf.xml";
#else
	filename = name_conf + "/" + "thermal-conf.xml";
#endif
	filename_auto = name_run + "/" + "thermal-conf.xml.auto";
	filename_auto_conf = name_conf + "/" + "thermal-conf.xml.auto";
}

int cthd_parse::parser_init(std::string config_file) {
	cthd_acpi_rel rel;
	const char *xml_config_file;
	int ret;

	if (config_file.empty()) {
		std::ifstream conf_auto(filename_auto_conf.c_str());
		if (conf_auto.is_open()) {
			thd_log_msg("Using generated %s\n", filename_auto_conf.c_str());
			xml_config_file = filename_auto_conf.c_str();
			auto_config = 1;
		} else {
			ret = rel.generate_conf(filename_auto);
			if (!ret) {
				thd_log_msg("Using generated %s\n", filename_auto.c_str());
				xml_config_file = filename_auto.c_str();
				auto_config = 1;
			} else {
				xml_config_file = filename.c_str();
			}
		}
	} else {
		xml_config_file = config_file.c_str();
	}

	/* We have not tested existence yet in this case. */
	if (!auto_config) {
		std::ifstream conf(xml_config_file);
		if (!conf.is_open()) {
			thd_log_msg("Config file %s does not exist\n", xml_config_file);
			return THD_ERROR;
		}
	}

	thd_log_msg("Using config file %s\n", xml_config_file);
	doc = xmlReadFile(xml_config_file, NULL, 0);
	if (doc == NULL) {
		thd_log_warn("error: could not parse file %s\n", xml_config_file);
		return THD_ERROR;
	}
	root_element = xmlDocGetRootElement(doc);

	if (root_element == NULL) {
		thd_log_warn("error: could not get root element\n");
		xmlFreeDoc(doc);
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_dependency_values(xmlNode * a_node, xmlDoc *doc,
		trip_cdev_depend_t *dependency) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n",
					cur_node->name,
					xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name,
						"CoolingDeviceType")) {
					dependency->cdev.assign(char_trim(tmp_value));
				} else if (!strcasecmp((const char*) cur_node->name,
						"CoolingDeviceState")) {
					dependency->state.assign(char_trim(tmp_value));
				}
				dependency->dependency = 1;
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_trip_cdev(xmlNode * a_node, xmlDoc *doc,
		trip_cdev_t *trip_cdev) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char *) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name, "type")) {
					trip_cdev->type.assign((const char*) tmp_value);
					string_trim(trip_cdev->type);
				} else if (!strcasecmp((const char*) cur_node->name, "influence")) {
					trip_cdev->influence = atoi(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"SamplingPeriod")) {
					trip_cdev->sampling_period = atoi(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"TargetState")) {
					trip_cdev->target_state = atoi(tmp_value);
					trip_cdev->target_state_valid = 1;
				} else if(!strcasecmp((const char*) cur_node->name,
						"PidControl")) {
					pid_control_t pid_params;

					parse_pid_values(cur_node->children, doc, &pid_params);
					trip_cdev->pid_param.kp = pid_params.Kp;
					trip_cdev->pid_param.ki = pid_params.Ki;
					trip_cdev->pid_param.kd = pid_params.Kd;
					trip_cdev->pid_param.valid = 1;
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_trip_point(xmlNode * a_node, xmlDoc *doc,
		trip_point_t *trip_pt) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	trip_cdev_t trip_cdev;
	trip_pt->temperature = 0;
	trip_pt->trip_pt_type = ACTIVE;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char *) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);

			if (!tmp_value) {
				continue;
			}

			if (!strcasecmp((const char*) cur_node->name, "Temperature")) {
				trip_pt->temperature = atoi(tmp_value);
			} else if (!strcasecmp((const char*) cur_node->name,
					"Power")) {
				trip_pt->temperature = atoi(tmp_value);
			} else if (!strcasecmp((const char*) cur_node->name, "Hyst")) {
				trip_pt->hyst = atoi(tmp_value);
			} else if (!strcasecmp((const char*) cur_node->name,
					"CoolingDevice")) {
				trip_cdev.influence = 0;
				trip_cdev.sampling_period = 0;
				trip_cdev.target_state_valid = 0;
				trip_cdev.target_state = 0;
				trip_cdev.type.clear();

				trip_cdev.pid_param.valid = 0;
				trip_cdev.pid_param.kp = 0.0;
				trip_cdev.pid_param.ki = 0.0;
				trip_cdev.pid_param.kd = 0.0;

				parse_new_trip_cdev(cur_node->children, doc, &trip_cdev);
				trip_pt->cdev_trips.push_back(trip_cdev);
			} else if (!strcasecmp((const char*) cur_node->name,
					"SensorType")) {
				trip_pt->sensor_type.assign(tmp_value);
				string_trim(trip_pt->sensor_type);
			} else if (!strcasecmp((const char*) cur_node->name, "type")) {
				char *type_val = char_trim(tmp_value);
				if (type_val && !strcasecmp(type_val, "active"))
					trip_pt->trip_pt_type = ACTIVE;
				else if (type_val && !strcasecmp(type_val, "passive"))
					trip_pt->trip_pt_type = PASSIVE;
				else if (type_val && !strcasecmp(type_val, "critical"))
					trip_pt->trip_pt_type = CRITICAL;
				else if (type_val && !strcasecmp(type_val, "hot"))
					trip_pt->trip_pt_type = HOT;
				else if (type_val && !strcasecmp(type_val, "max"))
					trip_pt->trip_pt_type = MAX;
			} else if (!strcasecmp((const char*) cur_node->name,
					"ControlType")) {
				char *ctrl_val = char_trim(tmp_value);
				if (ctrl_val && !strcasecmp(ctrl_val, "SEQUENTIAL"))
					trip_pt->control_type = SEQUENTIAL;
				else
					trip_pt->control_type = PARALLEL;
			} else if (!strcasecmp((const char*) cur_node->name, "DependsOn")) {
				parse_dependency_values(cur_node->children, doc,
						&trip_pt->dependency);
			}
			xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_trip_points(xmlNode * a_node, xmlDoc *doc,
		thermal_zone_t *info_ptr) {
	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name, "TripPoint")) {
				trip_point_t trip_pt;

				trip_pt.hyst = trip_pt.temperature = 0;
				trip_pt.trip_pt_type = PASSIVE;
				trip_pt.control_type = PARALLEL;
				trip_pt.influence = 100;
				trip_pt.sensor_type.clear();
				trip_pt.dependency.dependency = 0;
				if (parse_new_trip_point(cur_node->children, doc,
						&trip_pt) == THD_SUCCESS)
					info_ptr->trip_pts.push_back(trip_pt);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_pid_values(xmlNode * a_node, xmlDoc *doc,
		pid_control_t *pid_ptr) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	pid_ptr->Kp = 0.0005;
	pid_ptr->Ki = 0.0001;
	pid_ptr->Kd = 0.0001;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name, "Kp")) {
					pid_ptr->Kp = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name, "Kd")) {
					pid_ptr->Kd = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name, "Ki")) {
					pid_ptr->Ki = atof(tmp_value);
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_zone(xmlNode * a_node, xmlDoc *doc,
		thermal_zone_t *info_ptr) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (!strcasecmp((const char*) cur_node->name, "TripPoints")) {
				parse_trip_points(cur_node->children, doc, info_ptr);

			} else if (!strcasecmp((const char*) cur_node->name, "Type")) {
				info_ptr->type.assign((const char*) tmp_value);
				string_trim(info_ptr->type);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_cooling_dev(xmlNode * a_node, xmlDoc *doc,
		cooling_dev_t *cdev) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	cdev->max_state = cdev->min_state = 0;
	cdev->mask = 0;
	cdev->inc_dec_step = 1;
	cdev->read_back = true;
	cdev->auto_down_control = false;
	cdev->status = 0;
	cdev->pid_enable = false;
	cdev->unit_val = ABSOULUTE_VALUE;
	cdev->debounce_interval = 0;
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char *) cur_node->name, "Index")) {
					cdev->index = atoi(tmp_value);
				} else if (!strcasecmp((const char *) cur_node->name, "Type")) {
					cdev->type_string.assign((const char*) tmp_value);
					string_trim(cdev->type_string);
				} else if (!strcasecmp((const char *) cur_node->name, "Path")) {
					cdev->mask |= CDEV_DEF_BIT_PATH;
					cdev->path_str.assign((const char*) tmp_value);
					string_trim(cdev->path_str);
				} else if (!strcasecmp((const char *) cur_node->name, "MinState")) {
					cdev->mask |= CDEV_DEF_BIT_MIN_STATE;
					cdev->min_state = atoi(tmp_value);
				} else if (!strcasecmp((const char *) cur_node->name, "MaxState")) {
					cdev->mask |= CDEV_DEF_BIT_MAX_STATE;
					cdev->max_state = atoi(tmp_value);
				} else if (!strcasecmp((const char *) cur_node->name,
						"IncDecStep")) {
					cdev->mask |= CDEV_DEF_BIT_STEP;
					cdev->inc_dec_step = atoi(tmp_value);
				} else if (!strcasecmp((const char *) cur_node->name, "ReadBack")) {
					cdev->mask |= CDEV_DEF_BIT_READ_BACK;
					cdev->read_back = atoi(tmp_value);
				} else if (!strcasecmp((const char *) cur_node->name,
						"DebouncePeriod")) {
					cdev->mask |= CDEV_DEF_BIT_DEBOUNCE_VAL;
					cdev->debounce_interval = atoi(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"PidControl")) {
					cdev->mask |= CDEV_DEF_BIT_PID_PARAMS;
					cdev->pid_enable = true;
					parse_pid_values(cur_node->children, doc, &cdev->pid);
				} else if (!strcasecmp((const char *) cur_node->name,
						"AutoOffMode")) {
					cdev->mask |= CDEV_DEF_BIT_AUTO_DOWN;
					if (atoi(tmp_value))
						cdev->auto_down_control = true;
					else
						cdev->auto_down_control = false;
				} else if (!strcasecmp((const char *) cur_node->name,
						"WritePrefix")){
					cdev->mask |= CDEV_DEF_BIT_WRITE_PREFIX;
					cdev->write_prefix.assign((const char*) tmp_value);
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_cooling_devs(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;
	cooling_dev_t cdev;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name, "CoolingDevice")) {
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

int cthd_parse::parse_thermal_zones(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;
	thermal_zone_t zone;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name, "ThermalZone")) {
				zone.trip_pts.clear();
				parse_new_zone(cur_node->children, doc, &zone);
				info_ptr->zones.push_back(zone);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_sensor_link(xmlNode * a_node, xmlDoc *doc,
		thermal_sensor_link_t *info_ptr) {

	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name, "SensorType")) {
					info_ptr->name.assign(tmp_value);
					string_trim(info_ptr->name);
				} else if (!strcasecmp((const char*) cur_node->name,
						"Multiplier")) {
					info_ptr->multiplier = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name, "Offset")) {
					info_ptr->offset = atof(tmp_value);
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_sensor(xmlNode * a_node, xmlDoc *doc,
		thermal_sensor_t *info_ptr) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name, "Type")) {
					info_ptr->name.assign(tmp_value);
					string_trim(info_ptr->name);
				} else if (!strcasecmp((const char*) cur_node->name, "Path")) {
					info_ptr->mask |= SENSOR_DEF_BIT_PATH;
					info_ptr->path.assign(tmp_value);
					string_trim(info_ptr->path);
				} else if (!strcasecmp((const char*) cur_node->name,
						"AsyncCapable")) {
					info_ptr->async_capable = atoi(tmp_value);
					info_ptr->mask |= SENSOR_DEF_BIT_ASYNC_CAPABLE;
				} else if (!strcasecmp((const char*) cur_node->name, "Virtual")) {
					info_ptr->virtual_sensor = atoi(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"SensorLink")) {
					parse_new_sensor_link(cur_node->children, doc,
							&info_ptr->sensor_link);
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_thermal_sensors(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;
	thermal_sensor_t sensor;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name, "ThermalSensor")) {
				sensor.name.clear();
				sensor.path.clear();
				sensor.async_capable = false;
				sensor.mask = 0;
				sensor.virtual_sensor = false;
				parse_new_sensor(cur_node->children, doc, &sensor);
				info_ptr->sensors.push_back(sensor);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_platform_info(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	info_ptr->default_preference = PREF_ENERGY_CONSERVE;
	info_ptr->polling_interval = 0;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);

			if (!tmp_value) {
				continue;
			}

			if (!strcasecmp((const char*) cur_node->name, "uuid")) {
				info_ptr->uuid.assign((const char*) tmp_value);
				string_trim(info_ptr->uuid);
			} else if (!strcasecmp((const char*) cur_node->name,
					"ProductName")) {
				info_ptr->product_name.assign((const char*) tmp_value);
				string_trim(info_ptr->product_name);
			} else if (!strcasecmp((const char*) cur_node->name,
					"ProductSku")) {
				info_ptr->product_sku.assign((const char*) tmp_value);
				string_trim(info_ptr->product_sku);
			} else if (!strcasecmp((const char*) cur_node->name, "Name")) {
				info_ptr->name.assign((const char*) tmp_value);
				string_trim(info_ptr->name);
			} else if (!strcasecmp((const char*) cur_node->name,
					"Preference")) {
				char *pref_val = char_trim(tmp_value);
				if (pref_val && !strcasecmp(pref_val, "PERFORMANCE"))
					info_ptr->default_preference = PREF_PERFORMANCE;
				else
					info_ptr->default_preference = PREF_ENERGY_CONSERVE;
			} else if (!strcasecmp((const char*) cur_node->name,
					"ThermalZones")) {
				parse_thermal_zones(cur_node->children, doc, info_ptr);
			} else if (!strcasecmp((const char*) cur_node->name,
					"ThermalSensors")) {
				parse_thermal_sensors(cur_node->children, doc, info_ptr);
			} else if (!strcasecmp((const char*) cur_node->name,
					"CoolingDevices")) {
				parse_cooling_devs(cur_node->children, doc, info_ptr);
			} else if (!strcasecmp((const char*) cur_node->name,
				"PollingInterval")) {
				info_ptr->polling_interval = atoi(tmp_value);
			} else if (!strcasecmp((const char*) cur_node->name, "PPCC")) {
				parse_ppcc(cur_node->children, doc, &info_ptr->ppcc);
				info_ptr->ppcc.valid = 1;
			}
			xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_platform(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;
	unsigned char *tmp_value;
	thermal_info_t info = {};

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (unsigned char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (!strcasecmp((const char*) cur_node->name, "Platform")) {
				info.cooling_devs.clear();
				info.zones.clear();
				info.polling_interval = 0;
				info.ppcc.valid = 0;
				info.ppcc.limit_1_valid = 0;
				parse_new_platform_info(cur_node->children, doc, &info);
				thermal_info_list.push_back(info);
			}
			if (tmp_value)
				xmlFree(tmp_value);
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_ppcc(xmlNode * a_node, xmlDoc *doc, ppcc_t *ppcc) {
	xmlNode *cur_node = NULL;
	char *tmp_value;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			tmp_value = (char*) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				if (!strcasecmp((const char*) cur_node->name,
						"PowerLimitMinimum")) {
					ppcc->power_limit_min = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"PowerLimitMaximum")) {
					ppcc->power_limit_max = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"TimeWindowMinimum")) {
					ppcc->time_wind_min = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"TimeWindowMaximum")) {
					ppcc->time_wind_max = atof(tmp_value);
				} else if (!strcasecmp((const char*) cur_node->name,
						"StepSize")) {
					ppcc->step_size = atof(tmp_value);
				}
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse_new_thermal_conf(xmlNode * a_node, xmlDoc *doc,
		thermal_info_t *info_ptr) {
	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name,
					"ThermalConfiguration")) {
				parse_new_platform(cur_node->children, doc, info_ptr);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::parse(xmlNode * a_node, xmlDoc *doc) {
	xmlNode *cur_node = NULL;
	thermal_info_t info;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			DEBUG_PARSER_PRINT("node type: Element, name: %s value: %s\n", cur_node->name, xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1));
			if (!strcasecmp((const char*) cur_node->name,
					"ThermalConfiguration")) {
				parse_new_platform(cur_node->children, doc, &info);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_parse::start_parse() {
	parse(root_element, doc);

	return THD_SUCCESS;
}

void cthd_parse::parser_deinit() {
	for (unsigned int i = 0; i < thermal_info_list.size(); ++i) {
		thermal_info_list[i].sensors.clear();
		for (unsigned int j = 0; j < thermal_info_list[i].zones.size(); ++j) {
			thermal_info_list[i].zones[j].trip_pts.clear();
		}
		thermal_info_list[i].zones.clear();
		thermal_info_list[i].cooling_devs.clear();
	}
	xmlFreeDoc(doc);
}

void cthd_parse::dump_thermal_conf() {
	thd_log_info(" Dumping parsed XML Data\n");
	for (unsigned int i = 0; i < thermal_info_list.size(); ++i) {
		thd_log_info(" *** Index %u ***\n", i);
		thd_log_info("Name: %s\n", thermal_info_list[i].name.c_str());
		thd_log_info("Product Name: %s\n", thermal_info_list[i].product_name.c_str());
		thd_log_info("Product SKU: %s\n", thermal_info_list[i].product_sku.c_str());
		thd_log_info("UUID: %s\n", thermal_info_list[i].uuid.c_str());
		thd_log_info("type: %d\n", thermal_info_list[i].default_preference);
		thd_log_info("Polling Interval: %d seconds\n", thermal_info_list[i].polling_interval);

		if (thermal_info_list[i].ppcc.valid) {
			thd_log_info("PPCC: max_power_limit %d\n", thermal_info_list[i].ppcc.power_limit_max);
			thd_log_info("PPCC: min_power_limit %d\n", thermal_info_list[i].ppcc.power_limit_min);
			thd_log_info("PPCC: time_window_max %d\n", thermal_info_list[i].ppcc.time_wind_max);
			thd_log_info("PPCC: time_window_min %d\n", thermal_info_list[i].ppcc.time_wind_min);
			thd_log_info("PPCC: step_size %d\n", thermal_info_list[i].ppcc.step_size);
		}

		for (unsigned int j = 0; j < thermal_info_list[i].sensors.size(); ++j) {
			thd_log_info("\tSensor %u\n", j);
			thd_log_info("\t Name: %s\n",
					thermal_info_list[i].sensors[j].name.c_str());
			thd_log_info("\t Path: %s\n",
					thermal_info_list[i].sensors[j].path.c_str());
			thd_log_info("\t Async Capable: %d\n",
					thermal_info_list[i].sensors[j].async_capable);
			thd_log_info("\t Virtual: %d\n",
					thermal_info_list[i].sensors[j].virtual_sensor);
			if (thermal_info_list[i].sensors[j].virtual_sensor) {
				thd_log_info("\t\t Link type: %s\n",
						thermal_info_list[i].sensors[j].sensor_link.name.c_str());
				thd_log_info("\t\t Link mult: %f\n",
						thermal_info_list[i].sensors[j].sensor_link.multiplier);
				thd_log_info("\t\t Link offset: %f\n",
						thermal_info_list[i].sensors[j].sensor_link.offset);
			}
		}
		for (unsigned int j = 0; j < thermal_info_list[i].zones.size(); ++j) {
			thd_log_info("\tZone %u\n", j);
			thd_log_info("\t Name: %s\n",
					thermal_info_list[i].zones[j].type.c_str());
			for (unsigned int k = 0;
					k < thermal_info_list[i].zones[j].trip_pts.size(); ++k) {
				thd_log_info("\t\t Trip Point %u\n", k);
				thd_log_info("\t\t  temp/power %d\n",
						thermal_info_list[i].zones[j].trip_pts[k].temperature);
				thd_log_info("\t\t  trip type %d\n",
						thermal_info_list[i].zones[j].trip_pts[k].trip_pt_type);
				thd_log_info("\t\t  hyst id %d\n",
						thermal_info_list[i].zones[j].trip_pts[k].hyst);
				thd_log_info("\t\t  sensor type %s\n",
						thermal_info_list[i].zones[j].trip_pts[k].sensor_type.c_str());
				if (thermal_info_list[i].zones[j].trip_pts[k].dependency.dependency)
					thd_log_info("\t\t  Dependency on %s:%s\n",
							thermal_info_list[i].zones[j].trip_pts[k].dependency.cdev.c_str(),
							thermal_info_list[i].zones[j].trip_pts[k].dependency.state.c_str());

				for (unsigned int l = 0;
						l
								< thermal_info_list[i].zones[j].trip_pts[k].cdev_trips.size();
						++l) {
					thd_log_info("\t\t  cdev index %u\n", l);
					thd_log_info("\t\t\t  type %s\n",
							thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].type.c_str());
					thd_log_info("\t\t\t  influence %d\n",
							thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].influence);
					thd_log_info("\t\t\t  SamplingPeriod %d\n",
							thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].sampling_period);
					if (thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].target_state_valid)
						thd_log_info("\t\t\t  TargetState %d\n",
								thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].target_state);
					if (thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].pid_param.valid)
						thd_log_info("\t\t\t  PID values %f:%f:%f\n",
								thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].pid_param.kp,
								thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].pid_param.ki,
								thermal_info_list[i].zones[j].trip_pts[k].cdev_trips[l].pid_param.kd);
				}
			}
		}
		for (unsigned int l = 0; l < thermal_info_list[i].cooling_devs.size();
				++l) {
			thd_log_info("\tCooling Dev %u\n", l);
			thd_log_info("\t\tType: %s\n",
					thermal_info_list[i].cooling_devs[l].type_string.c_str());
			thd_log_info("\t\tPath: %s\n",
					thermal_info_list[i].cooling_devs[l].path_str.c_str());
			thd_log_info("\t\tMin: %d\n",
					thermal_info_list[i].cooling_devs[l].min_state);
			thd_log_info("\t\tMax: %d\n",
					thermal_info_list[i].cooling_devs[l].max_state);
			thd_log_info("\t\tStep: %d\n",
					thermal_info_list[i].cooling_devs[l].inc_dec_step);
			thd_log_info("\t\tReadBack: %d\n",
					thermal_info_list[i].cooling_devs[l].read_back);
			thd_log_info("\t\tDebouncePeriod: %d\n",
					thermal_info_list[i].cooling_devs[l].debounce_interval);
			thd_log_info("\t\tAutoDownControl: %d\n",
					thermal_info_list[i].cooling_devs[l].auto_down_control);
			if (thermal_info_list[i].cooling_devs[l].pid_enable) {
				thd_log_info("\t PID: Kp %f\n",
						thermal_info_list[i].cooling_devs[l].pid.Kp);
				thd_log_info("\t PID: Ki %f\n",
						thermal_info_list[i].cooling_devs[l].pid.Ki);
				thd_log_info("\t PID: Kd %f\n",
						thermal_info_list[i].cooling_devs[l].pid.Kd);
			}

		}
	}
}

int cthd_parse::get_polling_interval() {
	return thermal_info_list[matched_thermal_info_index].polling_interval;
}

bool cthd_parse::match_product_sku(int index) {
	std::string line;
	std::ifstream product_sku("/sys/class/dmi/id/product_sku");

	if (product_sku.is_open() && getline(product_sku, line)) {
		if (!thermal_info_list[index].product_sku.size())
			return true;
		string_trim(line);
		thd_log_debug("config product sku [%s] match with [%s]\n",
				thermal_info_list[index].product_sku.c_str(), line.c_str());
		if (thermal_info_list[index].product_sku == "*") {
			thd_log_info("Product Sku matched [wildcard]\n");
			return true;
		}
		if (line == thermal_info_list[index].product_sku) {
			thd_log_info("Product Sku matched\n");
			return true;
		}
	} else {
		return true;
	}

	return false;
}

bool cthd_parse::platform_matched() {

	std::string line;

	std::ifstream product_uuid("/sys/class/dmi/id/product_uuid");

	if (product_uuid.is_open() && getline(product_uuid, line)) {
		for (unsigned int i = 0; i < thermal_info_list.size(); ++i) {
			if (!thermal_info_list[i].uuid.size())
				continue;
			string_trim(line);
			thd_log_debug("config product uuid [%s] match with [%s]\n",
					thermal_info_list[i].uuid.c_str(), line.c_str());
			if (thermal_info_list[i].uuid == "*") {
				matched_thermal_info_index = i;
				thd_log_info("UUID matched [wildcard]\n");
				return true;
			}
			if (line == thermal_info_list[i].uuid) {
				matched_thermal_info_index = i;
				thd_log_info("UUID matched\n");
				return true;
			}
		}
	}

	std::ifstream product_name("/sys/class/dmi/id/product_name");

	if (product_name.is_open() && getline(product_name, line)) {
		for (unsigned int i = 0; i < thermal_info_list.size(); ++i) {
			if (!thermal_info_list[i].product_name.size())
				continue;
			string_trim(line);
			thd_log_debug("config product name [%s] match with [%s]\n",
					thermal_info_list[i].product_name.c_str(), line.c_str());
			if (thermal_info_list[i].product_name == "*") {
				matched_thermal_info_index = i;
				thd_log_info("Product Name matched [wildcard]\n");
				return true;
			}
			if (line == thermal_info_list[i].product_name) {
				if (!match_product_sku(i))
					continue;
				matched_thermal_info_index = i;
				thd_log_info("Product Name matched\n");
				return true;
			}
		}
	}
	for (unsigned int i = 0; i < thermal_info_list.size(); ++i) {
		if (!thermal_info_list[i].uuid.size())
			continue;
		if (!thermal_info_list[i].product_name.compare(0, 1, "*")) {
			matched_thermal_info_index = i;
			thd_log_info("Product Name matched\n");
			return true;
		}
	}

	return false;
}

int cthd_parse::trip_count(unsigned int zone_index) {
	if (zone_index
			< thermal_info_list[matched_thermal_info_index].zones.size()) {
		return thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts.size();
	} else
		return -1;

}

trip_point_t* cthd_parse::get_trip_point(unsigned int zone_index,
		unsigned int trip_index) {
	if (zone_index
			< thermal_info_list[matched_thermal_info_index].zones.size()) {
		if (trip_index
				< thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts.size())
			return &thermal_info_list[matched_thermal_info_index].zones[zone_index].trip_pts[trip_index];
		return NULL;
	} else
		return NULL;

}

cooling_dev_t* cthd_parse::get_cool_dev_index(unsigned int cdev_index) {

	if (cdev_index
			< thermal_info_list[matched_thermal_info_index].cooling_devs.size())
		return &thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index];
	else
		return NULL;
}

thermal_sensor_t* cthd_parse::get_sensor_dev_index(unsigned int sensor_index) {

	if (sensor_index
			< thermal_info_list[matched_thermal_info_index].sensors.size())
		return &thermal_info_list[matched_thermal_info_index].sensors[sensor_index];
	else
		return NULL;
}

thermal_zone_t *cthd_parse::get_zone_dev_index(unsigned int zone_index) {
	if (zone_index < thermal_info_list[matched_thermal_info_index].zones.size())
		return &thermal_info_list[matched_thermal_info_index].zones[zone_index];
	else
		return NULL;

}

ppcc_t *cthd_parse::get_ppcc_param(std::string name) {
	if (name != "TCPU.D0")
		return NULL;

	if (matched_thermal_info_index >= 0 && thermal_info_list[matched_thermal_info_index].ppcc.valid)
		return &thermal_info_list[matched_thermal_info_index].ppcc;

	return NULL;
}

bool cthd_parse::pid_status(int cdev_index) {
	return thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index].pid_enable;

}

bool cthd_parse::get_pid_values(int cdev_index, int *Kp, int *Ki, int *Kd) {
	if (thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index].pid_enable) {
		*Kp =
				thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index].pid.Kp;
		*Kd =
				thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index].pid.Kd;
		*Ki =
				thermal_info_list[matched_thermal_info_index].cooling_devs[cdev_index].pid.Ki;
		return true;
	}

	return false;
}

int cthd_parse::set_default_preference() {
	cthd_preference thd_pref;
	int ret;

	if (thermal_info_list[matched_thermal_info_index].default_preference
			== PREF_PERFORMANCE)
		ret = thd_pref.set_preference("PERFORMANCE");
	else
		ret = thd_pref.set_preference("ENERGY_CONSERVE");

	return ret;
}
