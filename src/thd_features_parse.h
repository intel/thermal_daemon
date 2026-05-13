/*
 * thd_features_parse.h: Parse features
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
#ifndef THD_FEATURES_PARSE_H
#define THD_FEATURES_PARSE_H

#include <cstdint>
#include <string>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "thermald.h"

typedef enum : uint8_t {
	DBUS_CONTROL,
	XML_THERMAL_CONFIG,
	DATA_VAULT_FS,
	KOBJECT_UEVENT_SUPPORT,
	MAX_FEATURE,
} thermald_feature_names_t;

class cthd_features_parse {
private:
	xmlDoc *doc;
	xmlNode *root_element;
	std::string filename;
	int parse_features(xmlNode *a_node, xmlDoc *doc);
	int parse(xmlNode *a_node, xmlDoc *doc);
	void set_feature_value(xmlNode *cur_node, xmlDoc *doc,
			thermald_feature_names_t feature);

public:
	std::vector<int> feature_list;
	char *char_trim(char *str);

	cthd_features_parse();
	int parser_init();
	void parser_deinit();

	int start_parse();
};

#endif
