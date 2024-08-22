/*
 * cthd_gddv.cpp: Adaptive thermal engine
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright 2020 Google LLC
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
 * Author Name Matthew Garrett <mjg59@google.com>
 *
 */
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include "thd_lzma_dec.h"
#include <linux/input.h>
#include <sys/types.h>
#include "thd_gddv.h"

/* From esif_lilb_datavault.h */
#define ESIFDV_NAME_LEN				32	// Max DataVault Name (Cache Name) Length (not including NULL)
#define ESIFDV_DESC_LEN				64	// Max DataVault Description Length (not including NULL)

#define SHA256_HASH_BYTES			32

struct header {
	uint16_t signature;
	uint16_t headersize;
	uint32_t version;

	union {
		/* Added in V1 */
		struct {
			uint32_t flags;
		} v1;

		/* Added in V2 */
		struct {
			uint32_t flags;
			char     segmentid[ESIFDV_NAME_LEN];
			char     comment[ESIFDV_DESC_LEN];
			uint8_t  payload_hash[SHA256_HASH_BYTES];
			uint32_t payload_size;
			uint32_t payload_class;
		} v2;
	};
} __attribute__ ((packed));

class _gddv_exception: public std::exception {
	virtual const char* what() const throw () {
		return "GDDV parsing failed";
	}
} gddv_exception;

void cthd_gddv::destroy_dynamic_sources() {
#ifndef ANDROID
	if (upower_client)
		g_clear_object(&upower_client);

	if (power_profiles_daemon)
		g_clear_object(&power_profiles_daemon);

	if (tablet_dev) {
		close(libevdev_get_fd(tablet_dev));
		libevdev_free(tablet_dev);

		if (lid_dev == tablet_dev)
			lid_dev = NULL;
		tablet_dev = NULL;
	}

	if (lid_dev) {
		close(libevdev_get_fd(lid_dev));
		libevdev_free(lid_dev);

		lid_dev = NULL;
	}
#endif

}

cthd_gddv::~cthd_gddv() {
}

int cthd_gddv::get_type(char *object, int *offset) {
	return *(uint32_t*) (object + *offset);
}

uint64_t cthd_gddv::get_uint64(char *object, int *offset) {
	uint64_t value;
	int type = *(uint32_t*) (object + *offset);

	if (type != 4) {
		thd_log_warn("Found object of type %d, expecting 4\n", type);
		throw gddv_exception;
	}
	*offset += 4;

	value = *(uint64_t*) (object + *offset);
	*offset += 8;

	return value;
}

char* cthd_gddv::get_string(char *object, int *offset) {
	int type = *(uint32_t*) (object + *offset);
	uint64_t length;
	char *value;

	if (type != 8) {
		thd_log_warn("Found object of type %d, expecting 8\n", type);
		throw gddv_exception;
	}
	*offset += 4;

	length = *(uint64_t*) (object + *offset);
	*offset += 8;

	value = &object[*offset];
	*offset += length;
	return value;
}

int cthd_gddv::merge_custom(struct custom_condition *custom,
		struct condition *condition) {
	condition->device = custom->participant;
	condition->condition = (enum adaptive_condition) custom->type;

	return 0;
}

int cthd_gddv::merge_appc() {
	for (int i = 0; i < (int) custom_conditions.size(); i++) {
		for (int j = 0; j < (int) conditions.size(); j++) {
			for (int k = 0; k < (int) conditions[j].size(); k++) {
				if (custom_conditions[i].condition
						== conditions[j][k].condition) {
					merge_custom(&custom_conditions[i], &conditions[j][k]);
				}
			}
		}
	}

	return 0;
}

int cthd_gddv::parse_appc(char *appc, int len) {
	int offset = 0;
	uint64_t version;

	if (appc[0] != 4) {
		thd_log_info("Found malformed APPC table, ignoring\n");
		return 0;
	}

	version = get_uint64(appc, &offset);
	if (version != 1) {
		// Invalid APPC tables aren't fatal
		thd_log_info("Found unsupported or malformed APPC version %d\n",
				(int) version);
		return 0;
	}

	while (offset < len) {
		struct custom_condition condition;

		condition.condition = (enum adaptive_condition) get_uint64(appc,
				&offset);
		condition.name = get_string(appc, &offset);
		condition.participant = get_string(appc, &offset);
		condition.domain = get_uint64(appc, &offset);
		condition.type = get_uint64(appc, &offset);
		custom_conditions.push_back(condition);
	}

	return 0;
}

int cthd_gddv::parse_apat(char *apat, int len) {
	int offset = 0;
	uint64_t version = get_uint64(apat, &offset);

	if (version != 2) {
		thd_log_warn("Found unsupported APAT version %d\n", (int) version);
		throw gddv_exception;
	}

	while (offset < len) {
		struct adaptive_target target;

		target.target_id = get_uint64(apat, &offset);
		target.name = get_string(apat, &offset);
		target.participant = get_string(apat, &offset);
		target.domain = get_uint64(apat, &offset);
		target.code = get_string(apat, &offset);
		target.argument = get_string(apat, &offset);
		targets.push_back(target);
	}
	return 0;
}

void cthd_gddv::dump_apat()
{
	thd_log_info("..apat dump begin..\n");
	for (unsigned int i = 0; i < targets.size(); ++i) {
		thd_log_info(
				"target_id:%" PRIu64 " name:%s participant:%s domain:%d code:%s argument:%s\n",
				targets[i].target_id, targets[i].name.c_str(),
				targets[i].participant.c_str(), (int)targets[i].domain,
				targets[i].code.c_str(), targets[i].argument.c_str());
	}
	thd_log_info("apat dump end\n");
}

int cthd_gddv::parse_apct(char *apct, int len) {
	int i;
	int offset = 0;
	uint64_t version = get_uint64(apct, &offset);

	if (version == 1) {
		while (offset < len) {
			std::vector<struct condition> condition_set;

			uint64_t target = get_uint64(apct, &offset);
			if (int(target) == -1) {
				thd_log_warn("Invalid APCT target\n");
				throw gddv_exception;
			}

			for (i = 0; i < 10; i++) {
				struct condition condition;

				condition.condition = adaptive_condition(0);
				condition.device = "";
				condition.comparison = adaptive_comparison(0);
				condition.argument = 0;
				condition.operation = adaptive_operation(0);
				condition.time_comparison = adaptive_comparison(0);
				condition.time = 0;
				condition.target = 0;
				condition.state = 0;
				condition.state_entry_time = 0;
				condition.target = target;
				condition.ignore_condition = 0;

				if (offset >= len) {
					thd_log_warn("Read off end of buffer in APCT parsing\n");
					throw gddv_exception;
				}

				condition.condition = adaptive_condition(
						get_uint64(apct, &offset));
				condition.comparison = adaptive_comparison(
						get_uint64(apct, &offset));
				condition.argument = get_uint64(apct, &offset);
				if (i < 9) {
					condition.operation = adaptive_operation(
							get_uint64(apct, &offset));
					if (condition.operation == FOR) {
						offset += 12;
						condition.time_comparison = adaptive_comparison(
								get_uint64(apct, &offset));
						condition.time = get_uint64(apct, &offset);
						offset += 12;
						i++;
					}
				}
				condition_set.push_back(condition);
			}
			conditions.push_back(condition_set);
		}
	} else if (version == 2) {
		while (offset < len) {
			std::vector<struct condition> condition_set;

			uint64_t target = get_uint64(apct, &offset);
			if (int(target) == -1) {
				thd_log_warn("Invalid APCT target");
				throw gddv_exception;
			}

			uint64_t count = get_uint64(apct, &offset);
			for (i = 0; i < int(count); i++) {
				struct condition condition = {};

				condition.condition = adaptive_condition(0);
				condition.device = "";
				condition.comparison = adaptive_comparison(0);
				condition.argument = 0;
				condition.operation = adaptive_operation(0);
				condition.time_comparison = adaptive_comparison(0);
				condition.time = 0;
				condition.target = 0;
				condition.state = 0;
				condition.state_entry_time = 0;
				condition.target = target;
				condition.ignore_condition = 0;

				if (offset >= len) {
					thd_log_warn("Read off end of buffer in parsing APCT\n");
					throw gddv_exception;
				}

				condition.condition = adaptive_condition(
						get_uint64(apct, &offset));
				condition.device = get_string(apct, &offset);
				offset += 12;
				condition.comparison = adaptive_comparison(
						get_uint64(apct, &offset));
				condition.argument = get_uint64(apct, &offset);
				if (i < int(count - 1)) {
					condition.operation = adaptive_operation(
							get_uint64(apct, &offset));
					if (condition.operation == FOR) {
						offset += 12;
						get_string(apct, &offset);
						offset += 12;
						condition.time_comparison = adaptive_comparison(
								get_uint64(apct, &offset));
						condition.time = get_uint64(apct, &offset);
						offset += 12;
						i++;
					}
				}
				condition_set.push_back(condition);
			}
			conditions.push_back(condition_set);
		}
	} else {
		thd_log_warn("Unsupported APCT version %d\n", (int) version);
		throw gddv_exception;
	}
	return 0;
}

static const char *condition_names[] = {
		"Invalid",
		"Default",
		"Orientation",
		"Proximity",
		"Motion",
		"Dock",
		"Workload",
		"Cooling_mode",
		"Power_source",
		"Aggregate_power_percentage",
		"Lid_state",
		"Platform_type",
		"Platform_SKU",
		"Utilisation",
		"TDP",
		"Duty_cycle",
		"Power",
		"Temperature",
		"Display_orientation",
		"Oem0",
		"Oem1",
		"Oem2",
		"Oem3",
		"Oem4",
		"Oem5",
		"PMAX",
		"PSRC",
		"ARTG",
		"CTYP",
		"PROP",
		"Unk1",
		"Unk2",
		"Battery_state",
		"Battery_rate",
		"Battery_remaining",
		"Battery_voltage",
		"PBSS",
		"Battery_cycles",
		"Battery_last_full",
		"Power_personality",
		"Battery_design_capacity",
		"Screen_state",
		"AVOL",
		"ACUR",
		"AP01",
		"AP02",
		"AP10",
		"Time",
		"Temperature_without_hysteresis",
		"Mixed_reality",
		"User_presence",
		"RBHF",
		"VBNL",
		"CMPP",
		"Battery_percentage",
		"Battery_count",
		"Power_slider"
};

static const char *comp_strs[] = {
		"INVALID",
		"ADAPTIVE_EQUAL",
		"ADAPTIVE_LESSER_OR_EQUAL",
		"ADAPTIVE_GREATER_OR_EQUAL"
};

#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(array[0]))

void cthd_gddv::dump_apct() {
	thd_log_info("..apct dump begin..\n");
	for (unsigned int i = 0; i < conditions.size(); ++i) {
		std::vector<struct condition> condition_set;

		thd_log_info("condition_set %u\n", i);
		condition_set = conditions[i];
		for (unsigned int j = 0; j < condition_set.size(); ++j) {
			std::string cond_name, comp_str, op_str;

			if (condition_set[j].condition < ARRAY_SIZE(condition_names)) {
				cond_name = condition_names[condition_set[j].condition];
			} else if (condition_set[j].condition >= 0x1000 && condition_set[j].condition < 0x10000) {
				std::stringstream msg;

				msg << "Oem" << (condition_set[j].condition - 0x1000 + 6);
				cond_name = msg.str();
			} else {
				std::stringstream msg;

				msg << "UNKNOWN" << "( " << condition_set[j].condition << " )";
				cond_name = msg.str();
			}

			if (condition_set[j].comparison < ARRAY_SIZE(comp_strs)) {
				comp_str = comp_strs[condition_set[j].comparison];
			}

			if (condition_set[j].operation == 1) {
				op_str = "AND";
			} else if (condition_set[j].operation == 2) {
				op_str = "FOR";
			} else {
				op_str = "INVALID";
			}

			thd_log_info(
					"\ttarget:%d device:%s condition:%s comparison:%s argument:%d"
							" operation:%s time_comparison:%d time:%ld"
							" stare:%d state_entry_time:%ld\n",
					condition_set[j].target, condition_set[j].device.c_str(),
					cond_name.c_str(), comp_str.c_str(),
					condition_set[j].argument, op_str.c_str(),
					condition_set[j].time_comparison, condition_set[j].time,
					condition_set[j].state, condition_set[j].state_entry_time);
		}
	}
	thd_log_info("..apct dump end..\n");
}

ppcc_t* cthd_gddv::get_ppcc_param(std::string name) {
	if (name != "TCPU.D0")
		return NULL;

	for (int i = 0; i < (int) ppccs.size(); i++) {
		if (ppccs[i].name == name)
			return &ppccs[i];
	}

	return NULL;
}

int cthd_gddv::parse_ppcc(char *name, char *buf, int len) {
	ppcc_t ppcc;

	ppcc.name = name;
	ppcc.power_limit_min = *(uint64_t*) (buf + 28);
	ppcc.power_limit_max = *(uint64_t*) (buf + 40);
	ppcc.time_wind_min = *(uint64_t*) (buf + 52);
	ppcc.time_wind_max = *(uint64_t*) (buf + 64);
	ppcc.step_size = *(uint64_t*) (buf + 76);
	ppcc.valid = 1;

	if (len < 156)
		return 0;

	thd_log_info("Processing ppcc limit 2, length %d\n", len);
	int start = 76 + 12;
	ppcc.power_limit_1_min = *(uint64_t*) (buf + start + 12);
	ppcc.power_limit_1_max = *(uint64_t*) (buf + start + 24);
	ppcc.time_wind_1_min = *(uint64_t*) (buf + start + 36);
	ppcc.time_wind_1_max = *(uint64_t*) (buf + start + 48);
	ppcc.step_1_size = *(uint64_t*) (buf + start + 60);

	if (ppcc.power_limit_1_max && ppcc.power_limit_1_min && ppcc.time_wind_1_min
			&& ppcc.time_wind_1_max && ppcc.step_1_size)
		ppcc.limit_1_valid = 1;
	else
		ppcc.limit_1_valid = 0;

	ppccs.push_back(ppcc);

	return 0;
}

void cthd_gddv::dump_ppcc()
{
	thd_log_info("..ppcc dump begin..\n");
	for (unsigned int i = 0; i < ppccs.size(); ++i) {
		thd_log_info(
				"Name:%s Limit:0 power_limit_max:%d power_limit_min:%d step_size:%d time_win_max:%d time_win_min:%d\n",
				ppccs[i].name.c_str(), ppccs[i].power_limit_max,
				ppccs[i].power_limit_min, ppccs[i].step_size,
				ppccs[i].time_wind_max, ppccs[i].time_wind_min);
		thd_log_info(
				"Name:%s Limit:1 power_limit_max:%d power_limit_min:%d step_size:%d time_win_max:%d time_win_min:%d\n",
				ppccs[i].name.c_str(), ppccs[i].power_limit_1_max,
				ppccs[i].power_limit_1_min, ppccs[i].step_1_size,
				ppccs[i].time_wind_1_max, ppccs[i].time_wind_1_min);
	}
	thd_log_info("ppcc dump end\n");
}

int cthd_gddv::parse_psvt(char *name, char *buf, int len) {
	int offset = 0;
	int version = get_uint64(buf, &offset);
	struct psvt psvt;

	if (version > 2) {
		thd_log_warn("Found unsupported PSVT version %d\n", (int) version);
		throw gddv_exception;
	}

	if (name == NULL)
		psvt.name = "Default";
	else
		psvt.name = name;
	while (offset < len) {
		struct psv psv;

		psv.source = get_string(buf, &offset);
		psv.target = get_string(buf, &offset);
		psv.priority = get_uint64(buf, &offset);
		psv.sample_period = get_uint64(buf, &offset);
		psv.temp = get_uint64(buf, &offset);
		psv.domain = get_uint64(buf, &offset);
		psv.control_knob = get_uint64(buf, &offset);
		if (get_type(buf, &offset) == 8) {
			psv.limit = get_string(buf, &offset);
		} else {
			uint64_t tmp = get_uint64(buf, &offset);
			psv.limit = std::to_string(tmp);
		}
		psv.step_size = get_uint64(buf, &offset);
		psv.limit_coeff = get_uint64(buf, &offset);
		psv.unlimit_coeff = get_uint64(buf, &offset);
		offset += 12;
		psvt.psvs.push_back(psv);
	}

	psvts.push_back(psvt);

	return 0;
}

void cthd_gddv::dump_psvt() {
	thd_log_info("..psvt dump begin..\n");
	for (unsigned int i = 0; i < psvts.size(); ++i) {
		std::vector<struct psv> psvs = psvts[i].psvs;

		thd_log_info("Name :%s\n", psvts[i].name.c_str());
		for (unsigned int j = 0; j < psvs.size(); ++j) {
			thd_log_info(
					"\t source:%s target:%s priority:%d sample_period:%d temp:%d domain:%d control_knob:%d psv.limit:%s\n",
					psvs[j].source.c_str(), psvs[j].target.c_str(),
					psvs[j].priority, psvs[j].sample_period,
					DECI_KELVIN_TO_CELSIUS(psvs[j].temp), psvs[j].domain,
					psvs[j].control_knob, psvs[j].limit.c_str());

		}
	}
	thd_log_info("psvt dump end\n");
}

struct psvt* cthd_gddv::find_def_psvt() {
	for (unsigned int i = 0; i < psvts.size(); ++i) {
		if (psvts[i].name == "IETM.D0") {
			return &psvts[i];
		}
	}

	return NULL;
}

int cthd_gddv::parse_itmt(char *name, char *buf, int len) {
	int offset = 0;
	int version = get_uint64(buf, &offset);
	struct itmt itmt;

	thd_log_debug(" ITMT version %d %s\n", (int) version, name);

	if (version > 2) {
		thd_log_info("Unsupported ITMT version\n");
		return THD_ERROR;
	}

	if (name == NULL)
		itmt.name = "Default";
	else
		itmt.name = name;

	while (offset < len) {
		struct itmt_entry itmt_entry;

		itmt_entry.target = get_string(buf, &offset);
		itmt_entry.trip_point = get_uint64(buf, &offset);
		itmt_entry.pl1_min = get_string(buf, &offset);
		itmt_entry.pl1_max = get_string(buf, &offset);
		itmt_entry.unused = get_string(buf, &offset);
		if (version == 2) {
			// Ref DPTF/Sources/Manager/DataManager.cpp DataManager::loadItmtTableObject()
			std::string dummy_str;
			unsigned long long dummy1,dummy2, dummy3;

			// There are three additional fields
			dummy1 = get_uint64(buf, &offset);
			dummy_str = get_string(buf, &offset);
			dummy2 = get_uint64(buf, &offset);
			dummy3 = get_uint64(buf, &offset);
			thd_log_debug("ignore dummy_str:%s %llu %llu %llu\n", dummy_str.c_str(), dummy1, dummy2, dummy3);
		} else {
			offset += 12;
		}

		itmt.itmt_entries.push_back(itmt_entry);
	}

	itmts.push_back(itmt);

	return 0;
}

void cthd_gddv::dump_itmt() {
	thd_log_info("..itmt dump begin..\n");
	for (unsigned int i = 0; i < itmts.size(); ++i) {
		std::vector<struct itmt_entry> itmt = itmts[i].itmt_entries;

		thd_log_info("Name :%s\n", itmts[i].name.c_str());
		for (unsigned int j = 0; j < itmt.size(); ++j) {
			thd_log_info("\t target:%s  trip_temp:%d pl1_min:%s pl1.max:%s\n",
					itmt[j].target.c_str(),
					DECI_KELVIN_TO_CELSIUS(itmt[j].trip_point),
					itmt[j].pl1_min.c_str(), itmt[j].pl1_max.c_str());
		}
	}
	thd_log_info("itmt dump end\n");
}

void cthd_gddv::parse_idsp(char *name, char *start, int length) {
	int len, i = 0;
	unsigned char *str = (unsigned char*) start;

	while (i < length) {
		char idsp[64];
		std::string idsp_str;

		// The minimum length for an IDSP should be at least 28
		// including headers and values
		if ((length - i) < 28)
			return;

		if (*str != 7)
			break;

		str += 4; // Get to Length field
		i += 4;

		len = *(int*) str;
		str += 8; // Get to actual contents
		i += 8;

		snprintf(idsp, sizeof(idsp),
				"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
				str[3], str[2], str[1], str[0], str[5], str[4], str[7], str[6],
				str[8], str[9], str[10], str[11], str[12], str[13], str[14],
				str[15]);

		idsp_str = idsp;
		std::transform(idsp_str.begin(), idsp_str.end(), idsp_str.begin(),
				::toupper);
		idsps.push_back(idsp_str);

		str += len;
		i += len;
	}
}

void cthd_gddv::dump_idsps() {
	thd_log_info("..idsp dump begin..\n");
	for (unsigned int i = 0; i < idsps.size(); ++i) {
		thd_log_info("idsp :%s\n", idsps[i].c_str());
	}
	thd_log_info("idsp dump end\n");
}

int cthd_gddv::search_idsp(std::string name)
{
	for (unsigned int i = 0; i < idsps.size(); ++i) {
		if (!idsps[i].compare(0, 36, name))
			return THD_SUCCESS;
	}

	return THD_ERROR;
}

void cthd_gddv::parse_trip_point(char *name, char *type, char *val, int len)
{
	struct trippoint trip;

	trip.name = name;
	trip.type_str = type;
	if (!trip.type_str.compare(0, 2, "_c"))
		trip.type = CRITICAL;
	else if (!trip.type_str.compare(0, 2, "_p"))
		trip.type = PASSIVE;
	else if (!trip.type_str.compare(0, 2, "_h"))
		trip.type = HOT;
	else if (!trip.type_str.compare(0, 2, "_a"))
		trip.type = ACTIVE;
	else
		trip.type = INVALID_TRIP_TYPE;
	trip.temp = DECI_KELVIN_TO_CELSIUS(*(int *)val);
	trippoints.push_back(trip);
}

void cthd_gddv::dump_trips() {
	thd_log_info("..trippoint dump begin..\n");
	for (unsigned int i = 0; i < trippoints.size(); ++i) {
		thd_log_info("name:%s type_str:%s type:%d temp:%d\n",
				trippoints[i].name.c_str(), trippoints[i].type_str.c_str(),
				trippoints[i].type, trippoints[i].temp);
	}
	thd_log_info("trippoint dump end\n");
}

int cthd_gddv::get_trip_temp(std::string name, trip_point_type_t type) {
	std::string search_name = name + ".D0";
	for (unsigned int i = 0; i < trippoints.size(); ++i) {
		if (!trippoints[i].name.compare(search_name)
				&& trippoints[i].type == type)
			return trippoints[i].temp;
	}

	return THD_ERROR;
}

int cthd_gddv::parse_trt(char *buf, int len)
{
	int offset = 0;

	thd_log_debug("TRT len:%d\n", len);

	if (len > 0) {
		thd_log_info(
				"_TRT not implemented. Report this for implementation with the thermald log using --loglevel=debug\n");
	}

	while (offset < len) {
		struct trt_entry entry;

		entry.source = get_string(buf, &offset);
		entry.dest = get_string(buf, &offset);
		entry.priority = get_uint64(buf, &offset);
		entry.sample_rate = get_uint64(buf, &offset);
		entry.resd0 = get_uint64(buf, &offset);
		entry.resd1 = get_uint64(buf, &offset);
		entry.resd2 = get_uint64(buf, &offset);
		entry.resd3 = get_uint64(buf, &offset);
		thd_log_info("trt source:%s dest:%s prio:%d sample_rate:%d\n",
				entry.source.c_str(), entry.dest.c_str(), entry.priority, entry.sample_rate);
	}

	return THD_SUCCESS;
}

// From Common/esif_sdk_iface_esif.h:
#define ESIF_SERVICE_CONFIG_COMPRESSED  0x40000000/* Payload is Compressed */
// From Common/esif_sdk.h
#define ESIFHDR_VERSION(major, minor, revision) ((uint32_t)((((major) & 0xFF) << 24) | (((minor) & 0xFF) << 16) | ((revision) & 0xFFFF)))
#define ESIFHDR_GET_MAJOR(version)	((uint32_t)(((version) >> 24) & 0xFF))
#define ESIFHDR_GET_MINOR(version)	((uint32_t)(((version) >> 16) & 0xFF))
#define ESIFHDR_GET_REVISION(version)	((uint32_t)((version) & 0xFFFF))
//From ESIF/Products/ESIF_LIB/Sources/esif_lib_datavault.c
#define ESIFDV_HEADER_SIGNATURE			0x1FE5
#define ESIFDV_ITEM_KEYS_REV0_SIGNATURE	0xA0D8

int cthd_gddv::handle_compressed_gddv(char *buf, int size) {
	struct header *header = (struct header*) buf;
	uint64_t payload_output_size;
	uint64_t output_size;
	int res;
	unsigned char *decompressed;
	size_t destlen=0;

	payload_output_size = *(uint64_t*) (buf + header->headersize + 5);
	output_size = header->headersize + payload_output_size;
	decompressed = (unsigned char*) malloc(output_size);

	if (!decompressed) {
		thd_log_warn("Failed to allocate buffer for decompressed output\n");
		throw gddv_exception;
	}

	res=lzma_decompress(NULL,&destlen, (const unsigned char*) (buf + header->headersize), size-header->headersize);

	thd_log_debug("decompress result =%d\n",res);

	res=lzma_decompress(( unsigned char*)(decompressed+ header->headersize),
	                &destlen,
	                (const unsigned char*) (buf + header->headersize),
	                size-header->headersize);

	thd_log_debug("decompress result =%d\n",res);

	/* Copy and update header.
	 * This will contain one or more nested repositories usually. */
	memcpy (decompressed, buf, header->headersize);
	header = (struct header*) decompressed;
	header->v2.flags &= ~ESIF_SERVICE_CONFIG_COMPRESSED;
	header->v2.payload_size = payload_output_size;

	res = parse_gddv((char*) decompressed, output_size, NULL);
	free(decompressed);

	return res;
}

int cthd_gddv::parse_gddv_key(char *buf, int size, int *end_offset) {
	int offset = 0;
	uint32_t keyflags;
	uint32_t keylength;
	uint32_t valtype;
	uint32_t vallength;
	char *key;
	char *val;
	char *str;
	char *name = NULL;
	char *type = NULL;
	char *point = NULL;
	char *ns = NULL;

	memcpy(&keyflags, buf + offset, sizeof(keyflags));
	offset += sizeof(keyflags);
	memcpy(&keylength, buf + offset, sizeof(keylength));
	offset += sizeof(keylength);
	key = new char[keylength];
	memcpy(key, buf + offset, keylength);
	offset += keylength;
	memcpy(&valtype, buf + offset, sizeof(valtype));
	offset += sizeof(valtype);
	memcpy(&vallength, buf + offset, sizeof(vallength));
	offset += sizeof(vallength);
	val = new char[vallength];
	memcpy(val, buf + offset, vallength);
	offset += vallength;

	if (end_offset)
		*end_offset = offset;

	str = strtok(key, "/");
	if (!str) {
		thd_log_debug("Ignoring key %s\n", key);

		delete[] (key);
		delete[] (val);

		/* Ignore */
		return THD_SUCCESS;
	}
	if (strcmp(str, "participants") == 0) {
		name = strtok(NULL, "/");
		type = strtok(NULL, "/");
		point = strtok(NULL, "/");
	} else if (strcmp(str, "shared") == 0) {
		ns = strtok(NULL, "/");
		type = strtok(NULL, "/");
		if (strcmp(ns, "tables") == 0) {
			point = strtok(NULL, "/");
		}
	}
	if (name && type && strcmp(type, "ppcc") == 0) {
		parse_ppcc(name, val, vallength);
	}

	if (type && strcmp(type, "psvt") == 0) {
		if (point == NULL)
			parse_psvt(name, val, vallength);
		else
			parse_psvt(point, val, vallength);
	}

	if (type && strcmp(type, "appc") == 0) {
		parse_appc(val, vallength);
	}

	if (type && strcmp(type, "apct") == 0) {
		parse_apct(val, vallength);
	}

	if (type && strcmp(type, "apat") == 0) {
		parse_apat(val, vallength);
	}

	if (type && strcmp(type, "itmt") == 0) {
		if (point == NULL)
			parse_itmt(name, val, vallength);
		else
			parse_itmt(point, val, vallength);
	}

	if (name && type && strcmp(type, "idsp") == 0) {
		parse_idsp(name, val, vallength);
	}

	if (name && type && point && strcmp(type, "trippoint") == 0) {
		parse_trip_point(name, point, val, vallength);
	}

	if (type && strcmp(type, "_trt") == 0) {
		parse_trt(val, vallength);
	}

	delete[] (key);
	delete[] (val);

	return THD_SUCCESS;
}

int cthd_gddv::parse_gddv(char *buf, int size, int *end_offset) {
	int offset = 0;
	struct header *header;

	if (size < (int) sizeof(struct header))
		return THD_ERROR;

	header = (struct header*) buf;

	if (header->signature != ESIFDV_HEADER_SIGNATURE) {
		thd_log_warn("Unexpected GDDV signature 0x%x\n", header->signature);
		throw gddv_exception;
	}
	if (ESIFHDR_GET_MAJOR(header->version) != 1
			&& ESIFHDR_GET_MAJOR(header->version) != 2)
		return THD_ERROR;

	offset = header->headersize;

	thd_log_debug("header version[%d] size[%d] header_size[%d] flags[%08X]\n",
			ESIFHDR_GET_MAJOR(header->version), size, header->headersize, header->v1.flags);

	if (ESIFHDR_GET_MAJOR(header->version) == 2) {
		char name[ESIFDV_NAME_LEN + 1] = { 0 };
		char comment[ESIFDV_DESC_LEN + 1] = { 0 };

		if (header->v2.flags & ESIF_SERVICE_CONFIG_COMPRESSED) {
			thd_log_debug("Uncompress GDDV payload\n");
			return handle_compressed_gddv(buf, size);
		}

		strncpy(name, header->v2.segmentid, sizeof(name) - 1);
		strncpy(comment, header->v2.comment, sizeof(comment) - 1);

		thd_log_debug("DV name: %s\n", name);
		thd_log_debug("DV comment: %s\n", comment);

		thd_log_debug("Got payload of size %d (data length: %d)\n", size, header->v2.payload_size);
		size = header->v2.payload_size;
	}

	while ((offset + header->headersize) < size) {
		int res;
		int end_offset = 0;

		if (ESIFHDR_GET_MAJOR(header->version) == 2) {
			unsigned short signature;

			signature = *(unsigned short *) (buf + offset);
			if (signature == ESIFDV_ITEM_KEYS_REV0_SIGNATURE) {
				offset += sizeof(unsigned short);
				res = parse_gddv_key(buf + offset, size - offset, &end_offset);
				if (res != THD_SUCCESS)
					return res;
				offset += end_offset;
			} else if (signature == ESIFDV_HEADER_SIGNATURE) {
				thd_log_info("Got subobject in buf %p at %d\n", buf, offset);
				res = parse_gddv(buf + offset, size - offset, &end_offset);
				if (res != THD_SUCCESS)
					return res;

				/* Parse recursively */
				offset += end_offset;
				thd_log_info("Subobject ended at %d of %d\n", offset, size);
			} else {
				thd_log_info("No known signature found 0x%04X\n", *(unsigned short *) (buf + offset));
				return THD_ERROR;
			}
		} else {
			res = parse_gddv_key(buf + offset, size - offset, &end_offset);
			if (res != THD_SUCCESS)
				return res;
			offset += end_offset;
		}
	}

	if (end_offset)
		*end_offset = offset;

	return 0;
}

int cthd_gddv::verify_condition(struct condition condition) {
	const char *cond_name;

	if (condition.condition >= Oem0 && condition.condition <= Oem5)
		return 0;
	if (condition.condition >= adaptive_condition(0x1000)
			&& condition.condition < adaptive_condition(0x10000))
		return 0;
	if (condition.condition == Default)
		return 0;
	if (condition.condition == Temperature
			|| condition.condition == Temperature_without_hysteresis
			|| condition.condition == (adaptive_condition) 0) {
		return 0;
	}
#ifndef ANDROID
	if (condition.condition == Lid_state && lid_dev != NULL)
		return 0;
	if (condition.condition == Power_source && upower_client != NULL)
		return 0;
#endif
	if (condition.condition == Workload)
		return 0;
	if (condition.condition == Platform_type)
		return 0;
	if (condition.condition == Power_slider)
		return 0;

	if ( condition.condition >=  ARRAY_SIZE(condition_names))
		cond_name = "UNKNOWN";
	else
		cond_name = condition_names[condition.condition];
	thd_log_info("Unsupported condition %" PRIu64 " (%s)\n", condition.condition, cond_name);

	return THD_ERROR;
}

int cthd_gddv::verify_conditions() {
	int result = 0;
	for (int i = 0; i < (int) conditions.size(); i++) {
		for (int j = 0; j < (int) conditions[i].size(); j++) {
			if (verify_condition(conditions[i][j]))
				result = THD_ERROR;
		}
	}

	if (result != 0)
		thd_log_info("Unsupported conditions are present\n");

	return result;
}

int cthd_gddv::compare_condition(struct condition condition,
		int value) {

	if (thd_engine && thd_engine->debug_mode_on()) {
		if (condition.condition < ARRAY_SIZE(condition_names)) {
			std::string cond_name, comp_str, op_str;

			cond_name = condition_names[condition.condition];
			if (condition.comparison < ARRAY_SIZE(comp_strs)) {
				comp_str = comp_strs[condition.comparison];
				thd_log_debug(
						"compare condition [%s] comparison [%s] value [%d]\n",
						cond_name.c_str(), comp_str.c_str(), value);
			} else {
				thd_log_debug(
						"compare condition [%s] comparison [%" PRIu64 "] value [%d]\n",
						cond_name.c_str(), condition.comparison, value);
			}
		} else {
			thd_log_debug("compare condition %" PRIu64 " value %d\n",
					condition.comparison, value);
		}
	}

	switch (condition.comparison) {
	case ADAPTIVE_EQUAL:
		if (value == condition.argument)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	case ADAPTIVE_LESSER_OR_EQUAL:
		if (value <= condition.argument)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	case ADAPTIVE_GREATER_OR_EQUAL:
		if (value >= condition.argument)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	default:
		return THD_ERROR;
	}
}

int cthd_gddv::compare_time(struct condition condition) {
	int elapsed = time(NULL) - condition.state_entry_time;

	switch (condition.time_comparison) {
	case ADAPTIVE_EQUAL:
		if (elapsed == condition.time)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	case ADAPTIVE_LESSER_OR_EQUAL:
		if (elapsed <= condition.time)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	case ADAPTIVE_GREATER_OR_EQUAL:
		if (elapsed >= condition.time)
			return THD_SUCCESS;
		else
			return THD_ERROR;
		break;
	default:
		return THD_ERROR;
	}
}

int cthd_gddv::evaluate_oem_condition(struct condition condition) {
	csys_fs sysfs(int3400_base_path.c_str());
	int oem_condition = -1;

	if (condition.condition >= Oem0 && condition.condition <= Oem5)
		oem_condition = (int) condition.condition - Oem0;
	else if (condition.condition >= (adaptive_condition) 0x1000
			&& condition.condition < (adaptive_condition) 0x10000)
		oem_condition = (int) condition.condition - 0x1000 + 6;

	if (oem_condition != -1) {
		std::string filename = "odvp" + std::to_string(oem_condition);
		std::string data;
		if (sysfs.read(filename, data) < 0) {
			thd_log_error("Unable to read %s\n", filename.c_str());
			return THD_ERROR;
		}
		int value = std::stoi(data, NULL);

		return compare_condition(std::move(condition), value);
	}

	return THD_ERROR;
}

int cthd_gddv::evaluate_temperature_condition(
		struct condition condition) {
	std::string sensor_name;

	if (condition.ignore_condition)
		return THD_ERROR;

	size_t pos = condition.device.find_last_of(".");
	if (pos == std::string::npos)
		sensor_name = condition.device;
	else
		sensor_name = condition.device.substr(pos + 1);

	cthd_sensor *sensor = thd_engine->search_sensor(std::move(sensor_name));
	if (!sensor) {
		thd_log_info("Unable to find a sensor for %s\n",
				condition.device.c_str());
		condition.ignore_condition = 1;
		return THD_ERROR;
	}

	int value = sensor->read_temperature();

	// Conditions are specified in decikelvin, temperatures are in
	// millicelsius.
	value = value / 100 + 2732;
	return compare_condition(std::move(condition), value);
}

#ifdef ANDROID
int cthd_gddv::evaluate_lid_condition(struct condition condition) {
        int value = 1;

        return compare_condition(condition, value);
}
#else
int cthd_gddv::evaluate_lid_condition(struct condition condition) {
	int value = 0;

	if (lid_dev) {
		struct input_event ev;

		while (libevdev_has_event_pending(lid_dev))
			libevdev_next_event(lid_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

		int lid_closed = libevdev_get_event_value(lid_dev, EV_SW, SW_LID);
		value = !lid_closed;
	}
	return compare_condition(std::move(condition), value);
}
#endif

int cthd_gddv::evaluate_workload_condition(
		struct condition condition) {
	// We don't have a good way to assert workload at the moment, so just
	// default to bursty

	return compare_condition(std::move(condition), 3);
}

#ifdef ANDROID
/*
 * Platform Type
 * Clamshell(1)
 * Tablet(2)
 * Other/Invalid(0)
 * */
int cthd_gddv::evaluate_platform_type_condition(
                struct condition condition) {
        int value = 2;//Tablet

        return compare_condition(condition, value);
}
#else
int cthd_gddv::evaluate_platform_type_condition(
		struct condition condition) {
	int value = 1;

	if (tablet_dev) {
		struct input_event ev;

		while (libevdev_has_event_pending(tablet_dev))
			libevdev_next_event(tablet_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

		int tablet = libevdev_get_event_value(tablet_dev, EV_SW,
				SW_TABLET_MODE);
		if (tablet)
			value = 2;
	}
	return compare_condition(std::move(condition), value);
}
#endif

int cthd_gddv::evaluate_power_slider_condition(
		struct condition condition) {

	return compare_condition(std::move(condition), power_slider);
}

#ifdef ANDROID
/*
 *Power Source
 AC(0)
 DC(1)
 Short Term DC(2)
 * */
int cthd_gddv::evaluate_ac_condition(struct condition condition) {
        csys_fs cdev_sysfs("/sys/class/power_supply/AC/online");
        std::string buffer;
        int status = 0;
        int value = 0;

        thd_log_debug("evaluate evaluate_ac_condition %" PRIu64 "\n", condition.condition);
        if (cdev_sysfs.exists("")) {
                        cdev_sysfs.read("", buffer);
                        std::istringstream(buffer) >> status;
        thd_log_debug("evaluate found battery sys status=%d\n",status);
        }
        if (status!=1) {
                value = 1;
        }

        thd_log_debug("evaluate found battery sys value=%d\n",value);
        return compare_condition(condition, value);
}
#else
int cthd_gddv::evaluate_ac_condition(struct condition condition) {
	int value = 0;
	bool on_battery = up_client_get_on_battery(upower_client);

	if (on_battery)
		value = 1;

	return compare_condition(std::move(condition), value);
}
#endif

int cthd_gddv::evaluate_condition(struct condition condition) {
	int ret = THD_ERROR;

	if (condition.condition == Default)
		return THD_SUCCESS;

	thd_log_debug("evaluate condition.condition %" PRIu64 "\n", condition.condition);

	if ((condition.condition >= Oem0 && condition.condition <= Oem5)
			|| (condition.condition >= (adaptive_condition) 0x1000
					&& condition.condition < (adaptive_condition) 0x10000))
		ret = evaluate_oem_condition(condition);

	if (condition.condition == Temperature
			|| condition.condition == Temperature_without_hysteresis
			|| condition.condition == (adaptive_condition) 0) {
		ret = evaluate_temperature_condition(condition);
	}

	if (condition.condition == Lid_state) {
		ret = evaluate_lid_condition(condition);
	}

	if (condition.condition == Power_source) {
		ret = evaluate_ac_condition(condition);
	}

	if (condition.condition == Workload) {
		ret = evaluate_workload_condition(condition);
	}

	if (condition.condition == Platform_type) {
		ret = evaluate_platform_type_condition(condition);
	}

	if (condition.condition == Power_slider) {
		ret = evaluate_power_slider_condition(condition);
	}

	if (condition.condition == Motion) {
		thd_log_debug("Match motion == 0 :%d\n", condition.argument);
		if (condition.argument == 0)
			ret = THD_SUCCESS;
	}

	if (ret) {
		if (condition.time && condition.state_entry_time == 0) {
			condition.state_entry_time = time(NULL);
		}
		ret = compare_time(std::move(condition));
	} else {
		condition.state_entry_time = 0;
	}

	return ret;
}

int cthd_gddv::evaluate_condition_set(
		std::vector<struct condition> condition_set) {
	for (int i = 0; i < (int) condition_set.size(); i++) {
		thd_log_debug("evaluate condition.condition at index %d\n", i);
		if (evaluate_condition(condition_set[i]) != 0)
			return THD_ERROR;
	}
	return THD_SUCCESS;
}

int cthd_gddv::evaluate_conditions() {
	int target = -1;

	for (int i = 0; i < (int) conditions.size(); i++) {
		thd_log_debug("evaluate condition set %d\n", i);
		if (evaluate_condition_set(conditions[i]) == THD_SUCCESS) {
			target = conditions[i][0].target;
			thd_log_debug("Condition Set matched:%d target:%d\n", i, target);

			break;
		}
	}

	return target;
}

struct psvt* cthd_gddv::find_psvt(std::string name) {
	for (int i = 0; i < (int) psvts.size(); i++) {
		if (!strcasecmp(psvts[i].name.c_str(), name.c_str())) {
			return &psvts[i];
		}
	}

	return NULL;
}

struct itmt* cthd_gddv::find_itmt(std::string name) {
	for (int i = 0; i < (int) itmts.size(); i++) {
		if (!strcasecmp(itmts[i].name.c_str(), name.c_str())) {
			return &itmts[i];
		}
	}

	return NULL;
}

int cthd_gddv::find_agressive_target() {
	int max_pl1_max = 0;
	int max_target_id = -1;

	for (int i = 0; i < (int) targets.size(); i++) {
		int argument;

		if (targets[i].code != "PL1MAX" && targets[i].code != "PL1PowerLimit")
			continue;

		try {
			argument = std::stoi(targets[i].argument, NULL);
		} catch (...) {
			thd_log_info("Invalid target target:%s %s\n",
					targets[i].code.c_str(), targets[i].argument.c_str());
			continue;
		}
		thd_log_info("target:%s %d\n", targets[i].code.c_str(), argument);

		if (max_pl1_max < argument) {
			max_pl1_max = argument;
			max_target_id = i;
		}
	}

	return max_target_id;
}

#ifndef ANDROID
void cthd_gddv::update_power_slider()
{
	g_autoptr(GVariant) active_profile_v = NULL;

	active_profile_v = g_dbus_proxy_get_cached_property (power_profiles_daemon, "ActiveProfile");
	if (active_profile_v && g_variant_is_of_type (active_profile_v, G_VARIANT_TYPE_STRING)) {
		const char *active_profile = g_variant_get_string (active_profile_v, NULL);

		if (strcmp (active_profile, "power-saver") == 0)
			power_slider = 25; /* battery saver */
		else if (strcmp (active_profile, "balanced") == 0)
			power_slider = 75; /* better performance */
		else if (strcmp (active_profile, "performance") == 0)
			power_slider = 100; /* best performance */
		else
			power_slider = 75;
	} else {
		power_slider = 75;
	}

	thd_log_info("Power slider is now set to %d\n", power_slider);
}

static void power_profiles_changed_cb(cthd_gddv *gddv)
{
	gddv->update_power_slider();
}
#endif

#ifndef ANDROID
static int is_event_device(const struct dirent *dir) {
	return strncmp("event", dir->d_name, 5) == 0;
}

void cthd_gddv::setup_input_devices() {
	struct dirent **namelist;
	int i, ndev, ret;

	ndev = scandir("/dev/input", &namelist, is_event_device, versionsort);
	for (i = 0; i < ndev; i++) {
		struct libevdev *dev = NULL;
		char fname[267];
		int fd = -1;

		snprintf(fname, sizeof(fname), "/dev/input/%s", namelist[i]->d_name);
		fd = open(fname, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		ret = libevdev_new_from_fd(fd, &dev);
		if (ret) {
			close(fd);
			continue;
		}

		if (!tablet_dev && libevdev_has_event_code(dev, EV_SW, SW_TABLET_MODE))
			tablet_dev = dev;
		if (!lid_dev &&  libevdev_has_event_code(dev, EV_SW, SW_LID))
			lid_dev = dev;

		if (lid_dev != dev && tablet_dev != dev) {
			libevdev_free(dev);
			close(fd);
		}
	}
}
#endif

//#define GDDV_LOAD_FROM_FILE

// Load a data_vault file from file system.
// Two formats are supported:
// 	data_vault.hex
// 		from "od -x" output from sysfs folder for gddv_dump
//	data_vault.bin
//		Binary as is.
// This is for test only and hence conditionally compiled
// This file is stored at TDCONFDIR

#ifdef GDDV_LOAD_FROM_FILE
#define MAX_GDDV_FILE_SIZE	(4 * 1024)

size_t cthd_gddv::gddv_load(char **buffer)
{
	std::string dir_name = TDCONFDIR;
	std::string file_name;
	ssize_t line_size;
	char *data_buffer;
	char *line_buffer = NULL;
	size_t line_buffer_size = 0;
	size_t data_buffer_index = 0;
	FILE *fp;

	file_name = dir_name + "/" + "data_vault.bin";

	fp = fopen(file_name.c_str(), "r");
	if (fp) {
		data_buffer = new char[MAX_GDDV_FILE_SIZE];
		if (!data_buffer) {
			return 0;
		}

		while (!feof(fp)) {
			unsigned char x;

			x = fgetc(fp);
			data_buffer[data_buffer_index++] = x;
		}
		fclose(fp);
		*buffer = data_buffer;
		return data_buffer_index;
	}

	file_name = dir_name + "/" + "data_vault.hex";

	fp = fopen(file_name.c_str(), "r");
	if (!fp)
		return 0;

	thd_log_debug("Found data_vault %s\n", file_name.c_str());

	data_buffer = new char[MAX_GDDV_FILE_SIZE];
	if (!data_buffer) {
		return 0;
	}

	line_size = getline(&line_buffer, &line_buffer_size, fp);

	while (line_size >= 0) {
		char s[2] = " ";
		char *token;

		/* get the first token */
		token = strtok(line_buffer, s);
		if (!token) {
			break;
		}

		while (token != NULL) {
			token = strtok(NULL, s);
			if (token) {
				int byte;

				sscanf(token, "%x", &byte);
				data_buffer[data_buffer_index++] = byte & 0xff;
				data_buffer[data_buffer_index++] = (byte & 0xff00) >> 8;
			}
		}

		line_size = getline(&line_buffer, &line_buffer_size, fp);
	}

	free(line_buffer);

	fclose(fp);

	*buffer = data_buffer;

	return data_buffer_index;
}

#else

size_t cthd_gddv::gddv_load(char **buffer)
{
	return 0;
}

#endif

int cthd_gddv::gddv_init(void) {
	csys_fs sysfs("");
	char *buf;
	size_t size;

	size = gddv_load(&buf);
	if (size > 0) {
		thd_log_info("Loading data vault from a file\n");
		goto skip_load;
	}

	if (sysfs.exists("/sys/bus/platform/devices/INT3400:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INT3400:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1040:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1040:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1041:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1041:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC10A0:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC10A0:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1042:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1042:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1068:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1068:00/";
	} else {
		return THD_ERROR;
	}

	if (sysfs.read(int3400_base_path + "firmware_node/path",
			int3400_path) < 0) {
		thd_log_debug("Unable to locate INT3400 firmware path\n");
		return THD_ERROR;
	}

	size = sysfs.size(int3400_base_path + "data_vault");
	if (size == 0) {
		thd_log_debug("Unable to open GDDV data vault\n");
		return THD_ERROR;
	}

	buf = new char[size];
	if (!buf) {
		thd_log_error("Unable to allocate memory for GDDV");
		return THD_FATAL_ERROR;
	}

	if (sysfs.read(int3400_base_path + "data_vault", buf, size)
			< int(size)) {
		thd_log_debug("Unable to read GDDV data vault\n");
		delete[] buf;
		return THD_FATAL_ERROR;
	}

skip_load:
	try {
		if (parse_gddv(buf, size, NULL)) {
			thd_log_debug("Unable to parse GDDV");
			delete[] buf;
			return THD_FATAL_ERROR;
		}

		merge_appc();

		dump_ppcc();
		dump_psvt();
		dump_itmt();
		dump_apat();
		dump_apct();
		dump_idsps();
		dump_trips();

		delete [] buf;
	} catch (std::exception &e) {
		thd_log_warn("%s\n", e.what());
		delete [] buf;
		return THD_FATAL_ERROR;
	}

#ifndef ANDROID
	setup_input_devices();

	upower_client = up_client_new();
	if (!upower_client) {
		thd_log_info("Unable to connect to upower\n");
		/* But continue to work */
	}

	g_autoptr(GDBusConnection) bus = NULL;

	bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
	if (bus) {
		power_profiles_daemon = g_dbus_proxy_new_sync (bus,
							       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
							       NULL,
							       "net.hadess.PowerProfiles",
							       "/net/hadess/PowerProfiles",
							       "net.hadess.PowerProfiles",
							       NULL,
							       NULL);

		if (power_profiles_daemon) {
			g_signal_connect_swapped (power_profiles_daemon,
						  "g-properties-changed",
						  (GCallback) power_profiles_changed_cb,
						  this);
			power_profiles_changed_cb(this);
		} else {
			thd_log_info("Could not setup DBus watch for power-profiles-daemon");
		}
	}
#endif

	return THD_SUCCESS;
}

void cthd_gddv::gddv_free(void)
{
	destroy_dynamic_sources();
}

