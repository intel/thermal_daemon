/*
 * thd_cdev_order_parser.cpp: Specify cdev order
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

#include "thd_cdev_order_parser.h"
#include "thd_sys_fs.h"

cthd_cdev_order_parse::cthd_cdev_order_parse() :
		doc(NULL), root_element(NULL) {
	std::string name = TDCONFDIR;
	filename = name + "/" "thermal-cpu-cdev-order.xml";
}

int cthd_cdev_order_parse::parser_init() {
	struct stat s;

	if (stat(filename.c_str(), &s))
		return THD_ERROR;

	doc = xmlReadFile(filename.c_str(), NULL, 0);
	if (doc == NULL) {
		thd_log_msg("error: could not parse file %s\n", filename.c_str());
		return THD_ERROR;
	}
	root_element = xmlDocGetRootElement(doc);

	if (root_element == NULL) {
		thd_log_warn("error: could not get root element\n");
		return THD_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_cdev_order_parse::start_parse() {
	parse(root_element, doc);

	return THD_SUCCESS;
}

void cthd_cdev_order_parse::parser_deinit() {
	xmlFreeDoc(doc);
}

int cthd_cdev_order_parse::parse_new_cdev(xmlNode * a_node, xmlDoc *doc) {
	xmlNode *cur_node = NULL;
	char *tmp_value;
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			tmp_value = (char *) xmlNodeListGetString(doc,
					cur_node->xmlChildrenNode, 1);
			if (tmp_value) {
				thd_log_info("node type: Element, name: %s value: %s\n",
						cur_node->name, tmp_value);
				cdev_order_list.push_back(tmp_value);
				xmlFree(tmp_value);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_cdev_order_parse::parse(xmlNode * a_node, xmlDoc *doc) {
	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if (!strcmp((const char*) cur_node->name, "CoolingDeviceOrder")) {
				parse_new_cdev(cur_node->children, doc);
			}
		}
	}

	return THD_SUCCESS;
}

int cthd_cdev_order_parse::get_order_list(std::vector<std::string> &list) {
	list = cdev_order_list;
	return 0;
}
