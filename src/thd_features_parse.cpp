/*
 * thd_feature_parse.cpp: Specify cdev order
 *
 * Copyright (C) 2026 Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include "thd_features_parse.h"
#include "thd_sys_fs.h"
#include "thd_util.h"

#include <cerrno>
#include <cstdlib>

static constexpr int thd_xml_parse_options = XML_PARSE_NONET | XML_PARSE_NOERROR
		| XML_PARSE_NOWARNING;

char *cthd_features_parse::char_trim(char *str)
{
	int i;

	if (!str)
		return nullptr;
	if (str[0] == '\0')
		return str;
	while (isspace(*str))
		str++;
	for (i = strlen(str) - 1; (isspace(str[i])); i--)
		;
	str[i + 1] = '\0';

	return str;
}

cthd_features_parse::cthd_features_parse() :
		doc(nullptr), root_element(nullptr) {
	std::string name = TDCONFDIR;
	filename = name + "/" "thermald-features.xml";
}

int cthd_features_parse::parser_init() {
	// Init all features as supported
	feature_list.assign(MAX_FEATURE, 1);

	int fd = open_validated_xml_file(filename);
	if (fd < 0)
		return THD_ERROR;

	doc = xmlReadFd(fd, filename.c_str(), nullptr, thd_xml_parse_options);
	close(fd);
	if (doc == nullptr) {
		thd_log_msg("error: could not parse file %s\n", filename.c_str());
		return THD_ERROR;
	}

	if (doc->intSubset != nullptr || doc->extSubset != nullptr) {
		thd_log_warn("Config file %s must not contain a DTD\n", filename.c_str());
		xmlFreeDoc(doc);
		doc = nullptr;
		return THD_ERROR;
	}

	root_element = xmlDocGetRootElement(doc);
	if (root_element == nullptr) {
		thd_log_warn("error: could not get root element\n");
		xmlFreeDoc(doc);
		doc = nullptr;
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

void cthd_features_parse::set_feature_value(xmlNode *cur_node, xmlDoc *doc, thermald_feature_names_t feature)
{
	char *tmp_value;

	if (feature >= MAX_FEATURE)
		return;

	tmp_value = (char *) xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
	if (tmp_value) {
		thd_log_debug("node type: Element, name: %s value: \"%s\" \n",
					(const char *)cur_node->name, tmp_value);

		errno = 0;
		char *endptr = nullptr;
		int value = strtol(char_trim(tmp_value), &endptr, 10);
		if (endptr != tmp_value && *endptr == '\0' && errno == 0) {
			feature_list[feature] = static_cast<int>(value);
		} else {
			thd_log_warn("Invalid feature value for %s: %s\n",
					(const char *)cur_node->name, tmp_value);
		}
		xmlFree(tmp_value);
	}
}

int cthd_features_parse::parse_features(xmlNode * a_node, xmlDoc *doc)
{
	xmlNode *cur_node = nullptr;
	bool parsed_any = false;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (!thd_strcasecmp_n((const char*) cur_node->name, "DbusControl")) {
				set_feature_value(cur_node, doc, DBUS_CONTROL);
				parsed_any = true;
				continue;
			}
			if (!thd_strcasecmp_n((const char*) cur_node->name, "XMLThermalConfig")) {
				set_feature_value(cur_node, doc, XML_THERMAL_CONFIG);
				parsed_any = true;
				continue;
			}
			if (!thd_strcasecmp_n((const char*) cur_node->name, "DataVaultFromFileSystem")) {
				set_feature_value(cur_node, doc, DATA_VAULT_FS);
				parsed_any = true;
				continue;
			}
			if (!thd_strcasecmp_n((const char*) cur_node->name, "KobjectUeventSupport")) {
				set_feature_value(cur_node, doc, KOBJECT_UEVENT_SUPPORT);
				parsed_any = true;
				continue;
			}
		}
	}

	return parsed_any ? THD_SUCCESS : THD_ERROR;
}

int cthd_features_parse::parse(xmlNode * a_node, xmlDoc *doc) {
	xmlNode *cur_node = nullptr;
	bool parsed_any = false;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (!thd_strcasecmp_n((const char*) cur_node->name, "ThermaldFeatures")) {
				if (parse_features(cur_node->children, doc) == THD_SUCCESS)
					parsed_any = true;
			}
		}
	}

	return parsed_any ? THD_SUCCESS : THD_ERROR;
}

int cthd_features_parse::start_parse() {
	return parse(root_element, doc);
}

void cthd_features_parse::parser_deinit() {
	xmlFreeDoc(doc);
	doc = nullptr;
	root_element = nullptr;
}
