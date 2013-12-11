/*
 * thd_cdev_order_parser.h: Specify cdev order
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
#ifndef THD_CDEV_ORDER_PARSE_H
#define THD_CDEV_ORDER_PARSE_H

#include <string>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "thermald.h"

class cthd_cdev_order_parse {
private:
	xmlDoc *doc;
	xmlNode *root_element;
	std::string filename;
	std::vector<std::string> cdev_order_list;

	int parse(xmlNode * a_node, xmlDoc *doc);
	int parse_new_cdev(xmlNode * a_node, xmlDoc *doc);

public:
	cthd_cdev_order_parse();
	int parser_init();
	void parser_deinit();

	int start_parse();
	int get_order_list(std::vector<std::string> &list);
};

#endif
