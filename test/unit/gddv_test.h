/*
 * Test access to cthd_gddv's parser (a friend of the class): parse a
 * vault buffer exactly like gddv_init() does, without touching sysfs,
 * D-Bus or input devices, and describe the result as JSON.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef THD_GDDV_TEST_H
#define THD_GDDV_TEST_H

#include "thd_gddv.h"

#include <cstdio>
#include <exception>
#include <sstream>
#include <string>

class cthd_gddv_test {
	static std::string q(const std::string &s)
	{
		std::string out = "\"";

		for (unsigned char c : s) {
			if (c == '"' || c == '\\') {
				out += '\\';
				out += c;
			} else if (c < 0x20 || c >= 0x7f) {
				char hex[8];
				snprintf(hex, sizeof(hex), "\\u%04x", c);
				out += hex;
			} else {
				out += c;
			}
		}
		return out + "\"";
	}

	static void dump(cthd_gddv &g, std::ostringstream &o)
	{
		o << "\"ppcc\": [";
		for (size_t i = 0; i < g.ppccs.size(); ++i) {
			const ppcc_t &p = g.ppccs[i];
			o << (i ? ", " : "") << "{\"name\": " << q(p.name)
			  << ", \"pl1_min\": " << p.power_limit_min
			  << ", \"pl1_max\": " << p.power_limit_max
			  << ", \"pl1_twin_min\": " << p.time_wind_min
			  << ", \"pl1_twin_max\": " << p.time_wind_max
			  << ", \"pl1_step\": " << p.step_size
			  << ", \"pl2_valid\": " << p.limit_1_valid
			  << ", \"pl2_min\": " << p.power_limit_1_min
			  << ", \"pl2_max\": " << p.power_limit_1_max << "}";
		}

		o << "],\n \"psvt\": {";
		for (size_t i = 0; i < g.psvts.size(); ++i) {
			o << (i ? ", " : "") << q(g.psvts[i].name) << ": [";
			for (size_t j = 0; j < g.psvts[i].psvs.size(); ++j) {
				const psv &p = g.psvts[i].psvs[j];
				o << (j ? ", " : "") << "{\"source\": " << q(p.source)
				  << ", \"target\": " << q(p.target)
				  << ", \"temp\": " << p.temp
				  << ", \"limit\": " << q(p.limit)
				  << ", \"sample_period\": " << p.sample_period
				  << ", \"priority\": " << p.priority << "}";
			}
			o << "]";
		}

		o << "},\n \"apct\": [";
		for (size_t i = 0; i < g.conditions.size(); ++i) {
			o << (i ? ", " : "") << "[";
			for (size_t j = 0; j < g.conditions[i].size(); ++j) {
				const condition &c = g.conditions[i][j];
				o << (j ? ", " : "") << "{\"target\": " << c.target
				  << ", \"condition\": " << c.condition
				  << ", \"device\": " << q(c.device)
				  << ", \"comparison\": " << c.comparison
				  << ", \"argument\": " << c.argument << "}";
			}
			o << "]";
		}

		o << "],\n \"apat\": [";
		for (size_t i = 0; i < g.targets.size(); ++i) {
			const adaptive_target &t = g.targets[i];
			o << (i ? ", " : "") << "{\"target_id\": " << t.target_id
			  << ", \"name\": " << q(t.name)
			  << ", \"participant\": " << q(t.participant)
			  << ", \"code\": " << q(t.code)
			  << ", \"argument\": " << q(t.argument) << "}";
		}

		o << "],\n \"itmt\": [";
		for (size_t i = 0; i < g.itmts.size(); ++i) {
			o << (i ? ", " : "") << "{\"name\": " << q(g.itmts[i].name)
			  << ", \"entries\": [";
			for (size_t j = 0; j < g.itmts[i].itmt_entries.size(); ++j) {
				const itmt_entry &e = g.itmts[i].itmt_entries[j];
				o << (j ? ", " : "") << "{\"target\": " << q(e.target)
				  << ", \"trip_point\": " << e.trip_point
				  << ", \"pl1_min\": " << q(e.pl1_min)
				  << ", \"pl1_max\": " << q(e.pl1_max) << "}";
			}
			o << "]}";
		}

		o << "],\n \"vsensors\": {";
		for (size_t i = 0; i < g.vsensors.size(); ++i) {
			const vsensor &v = g.vsensors[i];
			o << (i ? ", " : "") << q(v.name) << ": {\"vsct\": [";
			for (size_t j = 0; j < v.vscts.size(); ++j)
				o << (j ? ", " : "") << "{\"target\": " << q(v.vscts[j].target)
				  << ", \"coeff\": " << v.vscts[j].coeff
				  << ", \"alpha\": " << v.vscts[j].alpha << "}";
			o << "], \"vspt\": [";
			for (size_t j = 0; j < v.vspts.size(); ++j)
				o << (j ? ", " : "") << "{\"temp\": " << v.vspts[j].virtual_temp
				  << ", \"period\": " << v.vspts[j].sample_period << "}";
			o << "]}";
		}

		o << "},\n \"idsp\": [";
		for (size_t i = 0; i < g.idsps.size(); ++i)
			o << (i ? ", " : "") << q(g.idsps[i]);
		o << "]";
	}

public:
	/* Returns 0 when the parser accepted the vault. Never throws. */
	static int parse_and_dump(char *buf, size_t size, std::string &json)
	{
		cthd_gddv g;
		std::ostringstream o;
		int ret;

		try {
			ret = g.parse_gddv(buf, (int) size, nullptr, 0);
			if (!ret)
				g.merge_appc();
		} catch (std::exception &e) {
			o << "{\"status\": \"error\", \"error\": " << q(e.what()) << "}\n";
			json = o.str();
			return 1;
		}

		o << "{\"status\": " << (ret ? "\"error\"" : "\"ok\"") << ",\n ";
		dump(g, o);
		o << "}\n";
		json = o.str();

		return ret ? 1 : 0;
	}
};

#endif
