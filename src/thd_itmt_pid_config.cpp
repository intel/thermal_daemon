/*
 * thd_itmt_pid_config.cpp: PID control of ITMT trips, enabled and tuned in
 * TDCONFDIR/itmt-pid.xml
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

#include "thd_itmt_pid_config.h"
#include "thermald.h"
#include "thd_util.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>

static constexpr int thd_xml_parse_options = XML_PARSE_NONET | XML_PARSE_NOERROR
		| XML_PARSE_NOWARNING;

void cthd_itmt_pid_config::load() {
	std::string dir = TDCONFDIR;

	load(dir + "/" "itmt-pid.xml");
}

void cthd_itmt_pid_config::load(const std::string &filename) {
	if (access(filename.c_str(), F_OK))
		return;

	int fd = open_validated_xml_file(filename);
	if (fd < 0)
		return;

	xmlDoc *doc = xmlReadFd(fd, filename.c_str(), nullptr, thd_xml_parse_options);
	close(fd);
	if (!doc) {
		thd_log_warn("%s: could not parse, ITMT PID disabled\n",
				filename.c_str());
		return;
	}
	if (doc->intSubset != nullptr || doc->extSubset != nullptr) {
		thd_log_warn("Config file %s must not contain a DTD\n", filename.c_str());
		xmlFreeDoc(doc);
		return;
	}

	xmlNode *root = xmlDocGetRootElement(doc);

	if (!root || thd_strcasecmp_n((const char *) root->name, "ItmtPid")) {
		thd_log_warn("%s: no ItmtPid element, ITMT PID disabled\n",
				filename.c_str());
		xmlFreeDoc(doc);
		return;
	}

	for (xmlNode *node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		const char *name = (const char *) node->name;
		char *value = (char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

		if (!value) {
			thd_log_warn("%s: %s has no value, ignored\n", filename.c_str(), name);
			continue;
		}

		std::string str(value);
		xmlFree(value);

		double d;
		int i;
		int ret;

		if (!thd_strcasecmp_n(name, "Enable")) {
			ret = parse_int_value(str, &i, 0, 1);
			if (ret == THD_SUCCESS)
				enable = i;
		} else if (!thd_strcasecmp_n(name, "Kp")) {
			ret = parse_double_value(str, &d, 0, 1e6);
			if (ret == THD_SUCCESS)
				kp = d;
		} else if (!thd_strcasecmp_n(name, "Kd")) {
			ret = parse_double_value(str, &d, 0, 1e7);
			if (ret == THD_SUCCESS)
				kd = d;
		} else if (!thd_strcasecmp_n(name, "SetPointOffset")) {
			ret = parse_int_value(str, &i, 0, 5000);
			if (ret == THD_SUCCESS)
				set_point_offset = i;
		} else if (!thd_strcasecmp_n(name, "ScaleWithRange")) {
			ret = parse_int_value(str, &i, 0, 1);
			if (ret == THD_SUCCESS)
				scale_with_range = i;
		} else if (!thd_strcasecmp_n(name, "ReferenceRange")) {
			ret = parse_int_value(str, &i, 1000, 1000000);
			if (ret == THD_SUCCESS)
				reference_range_mw = i;
		} else if (!thd_strcasecmp_n(name, "ScaleMin")) {
			ret = parse_double_value(str, &d, 0.1, 10);
			if (ret == THD_SUCCESS)
				scale_min = d;
		} else if (!thd_strcasecmp_n(name, "ScaleMax")) {
			ret = parse_double_value(str, &d, 0.1, 10);
			if (ret == THD_SUCCESS)
				scale_max = d;
		} else {
			thd_log_warn("%s: unknown element %s, ignored\n", filename.c_str(), name);
			continue;
		}

		if (ret != THD_SUCCESS)
			thd_log_warn("%s: invalid %s \"%s\", ignored\n", filename.c_str(),
					name, str.c_str());
	}
	xmlFreeDoc(doc);

	if (scale_min > scale_max) {
		thd_log_warn("%s: ScaleMin %g above ScaleMax %g, using %g-%g\n",
				filename.c_str(), scale_min, scale_max, ITMT_PID_SCALE_MIN,
				ITMT_PID_SCALE_MAX);
		scale_min = ITMT_PID_SCALE_MIN;
		scale_max = ITMT_PID_SCALE_MAX;
	}

	thd_log_info("%s: ITMT PID %s, Kp %g Kd %g set point offset %d scale %s"
			" (reference %d mW, %g-%g)\n", filename.c_str(),
			enable ? "enabled" : "disabled", kp, kd,
			set_point_offset, scale_with_range ? "with range" : "off",
			reference_range_mw, scale_min, scale_max);
}

double cthd_itmt_pid_config::scale(long long range_uw) const {
	if (!scale_with_range || range_uw <= 0)
		return 1.0;

	double s = (double) range_uw / ((double) reference_range_mw * 1000.0);

	if (s < scale_min)
		s = scale_min;
	if (s > scale_max)
		s = scale_max;

	return s;
}
