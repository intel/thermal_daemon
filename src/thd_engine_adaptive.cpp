/*
 * cthd_engine_adaptive.cpp: Adaptive thermal engine
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
#include <lzma.h>
#include <linux/input.h>
#include <sys/types.h>
#include "thd_engine_adaptive.h"
#include "thd_zone_cpu.h"
#include "thd_zone_generic.h"
#include "thd_cdev_gen_sysfs.h"
#include "thd_cdev_cpufreq.h"
#include "thd_cdev_rapl.h"
#include "thd_cdev_intel_pstate_driver.h"
#include "thd_cdev_rapl_dram.h"
#include "thd_sensor_virtual.h"
#include "thd_cdev_backlight.h"
#include "thd_int3400.h"
#include "thd_sensor_kbl_amdgpu_thermal.h"
#include "thd_sensor_kbl_amdgpu_power.h"
#include "thd_cdev_kbl_amdgpu.h"
#include "thd_zone_kbl_amdgpu.h"
#include "thd_sensor_kbl_g_mcp.h"
#include "thd_zone_kbl_g_mcp.h"
#include "thd_cdev_kbl_amdgpu.h"
#include "thd_zone_kbl_g_mcp.h"

#ifdef GLIB_SUPPORT
#include "thd_cdev_modem.h"
#endif

/* From esif_lilb_datavault.h */
#define ESIFDV_NAME_LEN				32			// Max DataVault Name (Cache Name) Length (not including NUL)
#define ESIFDV_DESC_LEN				64			// Max DataVault Description Length (not including NUL)

#define SHA256_HASH_BYTES				32

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

cthd_engine_adaptive::~cthd_engine_adaptive() {
}

int cthd_engine_adaptive::get_type(char *object, int *offset) {
	return *(uint32_t*) (object + *offset);
}

uint64_t cthd_engine_adaptive::get_uint64(char *object, int *offset) {
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

char* cthd_engine_adaptive::get_string(char *object, int *offset) {
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

int cthd_engine_adaptive::merge_custom(struct custom_condition *custom,
		struct condition *condition) {
	condition->device = custom->participant;
	condition->condition = (enum adaptive_condition) custom->type;

	return 0;
}

int cthd_engine_adaptive::merge_appc() {
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

int cthd_engine_adaptive::parse_appc(char *appc, int len) {
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

int cthd_engine_adaptive::parse_apat(char *apat, int len) {
	int offset = 0;
	uint64_t version = get_uint64(apat, &offset);

	if (version != 2) {
		thd_log_warn("Found unsupported APAT version %d\n", (int) version);
		throw gddv_exception;
	}

	while (offset < len) {
		struct adaptive_target target;

		if (offset >= len)
			return THD_ERROR;

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

void cthd_engine_adaptive::dump_apat()
{
	thd_log_info("..apat dump begin.. \n");
	for (unsigned int i = 0; i < targets.size(); ++i) {
		thd_log_info(
				"target_id:%" PRIu64 " name:%s participant:%s domain:%d code:%s argument:%s\n",
				targets[i].target_id, targets[i].name.c_str(),
				targets[i].participant.c_str(), (int)targets[i].domain,
				targets[i].code.c_str(), targets[i].argument.c_str());
	}
	thd_log_info("apat dump end\n");
}

int cthd_engine_adaptive::parse_apct(char *apct, int len) {
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

void cthd_engine_adaptive::dump_apct() {
	thd_log_info("..apct dump begin.. \n");
	for (unsigned int i = 0; i < conditions.size(); ++i) {
		std::vector<struct condition> condition_set;

		thd_log_info("condition_set %d\n", i);
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
							" operation:%s time_comparison:%d time:%d"
							" stare:%d state_entry_time:%d \n",
					condition_set[j].target, condition_set[j].device.c_str(),
					cond_name.c_str(), comp_str.c_str(),
					condition_set[j].argument, op_str.c_str(),
					condition_set[j].time_comparison, condition_set[j].time,
					condition_set[j].state, condition_set[j].state_entry_time);
		}
	}
	thd_log_info("..apct dump end.. \n");
}

ppcc_t* cthd_engine_adaptive::get_ppcc_param(std::string name) {
	for (int i = 0; i < (int) ppccs.size(); i++) {
		if (ppccs[i].name == name)
			return &ppccs[i];
	}

	return NULL;
}

int cthd_engine_adaptive::parse_ppcc(char *name, char *buf, int len) {
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

void cthd_engine_adaptive::dump_ppcc()
{
	thd_log_info("..ppcc dump begin.. \n");
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

int cthd_engine_adaptive::parse_psvt(char *name, char *buf, int len) {
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

#define DECI_KELVIN_TO_CELSIUS(t)       ({                      \
        int _t = (t);                                          \
        ((_t-2732 >= 0) ? (_t-2732+5)/10 : (_t-2732-5)/10);     \
})

void cthd_engine_adaptive::dump_psvt() {
	thd_log_info("..psvt dump begin.. \n");
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

struct psvt* cthd_engine_adaptive::find_def_psvt() {
	for (unsigned int i = 0; i < psvts.size(); ++i) {
		if (psvts[i].name == "IETM.D0") {
			return &psvts[i];
		}
	}

	return NULL;
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

int cthd_engine_adaptive::handle_compressed_gddv(char *buf, int size) {
	struct header *header = (struct header*) buf;
	uint64_t payload_output_size;
	uint64_t output_size;
	lzma_ret ret;
	int res;
	unsigned char *decompressed;
	lzma_stream strm = LZMA_STREAM_INIT;

	payload_output_size = *(uint64_t*) (buf + header->headersize + 5);
	output_size = header->headersize + payload_output_size;
	decompressed = (unsigned char*) malloc(output_size);

	if (!decompressed) {
		thd_log_warn("Failed to allocate buffer for decompressed output\n");
		throw gddv_exception;
	}
	ret = lzma_auto_decoder(&strm, 64 * 1024 * 1024, 0);
	if (ret) {
		thd_log_warn("Failed to initialize LZMA decoder: %d\n", ret);
		free(decompressed);
		throw gddv_exception;
	}
	strm.next_out = decompressed + header->headersize;
	strm.avail_out = output_size;
	strm.next_in = (const unsigned char*) (buf + header->headersize);
	strm.avail_in = size;
	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);
	if (ret && ret != LZMA_STREAM_END) {
		thd_log_warn("Failed to decompress GDDV data: %d\n", ret);
		free(decompressed);
		throw gddv_exception;
	}

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

int cthd_engine_adaptive::parse_gddv_key(char *buf, int size, int *end_offset) {
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

	delete[] (key);
	delete[] (val);

	return THD_SUCCESS;
}

int cthd_engine_adaptive::parse_gddv(char *buf, int size, int *end_offset) {
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

		if (header->v2.flags == ESIF_SERVICE_CONFIG_COMPRESSED) {
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

int cthd_engine_adaptive::verify_condition(struct condition condition) {
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
	if (condition.condition == Lid_state && upower_client != NULL)
		return 0;
	if (condition.condition == Power_source && upower_client != NULL)
		return 0;
	if (condition.condition == Workload)
		return 0;
	if (condition.condition == Platform_type)
		return 0;
	if (condition.condition == Power_slider)
		return 0;

	if ( condition.condition >=  ARRAY_SIZE(condition_names))
		cond_name = "UKNKNOWN";
	else
		cond_name = condition_names[condition.condition];
	thd_log_error("Unsupported condition %" PRIu64 " (%s)\n", condition.condition, cond_name);

	return THD_ERROR;
}

int cthd_engine_adaptive::verify_conditions() {
	int result = 0;
	for (int i = 0; i < (int) conditions.size(); i++) {
		for (int j = 0; j < (int) conditions[i].size(); j++) {
			if (verify_condition(conditions[i][j]))
				result = THD_ERROR;
		}
	}

	if (result != 0)
		thd_log_error("Unsupported conditions are present\n");

	return result;
}

int cthd_engine_adaptive::compare_condition(struct condition condition,
		int value) {

	if (debug_mode_on()) {
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

int cthd_engine_adaptive::compare_time(struct condition condition) {
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

int cthd_engine_adaptive::evaluate_oem_condition(struct condition condition) {
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

		return compare_condition(condition, value);
	}

	return THD_ERROR;
}

int cthd_engine_adaptive::evaluate_temperature_condition(
		struct condition condition) {
	std::string sensor_name;

	size_t pos = condition.device.find_last_of(".");
	if (pos == std::string::npos)
		sensor_name = condition.device;
	else
		sensor_name = condition.device.substr(pos + 1);

	cthd_sensor *sensor = search_sensor(sensor_name);
	if (!sensor) {
		thd_log_warn("Unable to find a sensor for %s\n",
				condition.device.c_str());
		return THD_ERROR;
	}

	int value = sensor->read_temperature();

	// Conditions are specified in decikelvin, temperatures are in
	// millicelsius.
	value = value / 100 + 2732;
	return compare_condition(condition, value);
}

int cthd_engine_adaptive::evaluate_lid_condition(struct condition condition) {
	int value = 0;
	bool lid_closed = up_client_get_lid_is_closed(upower_client);

	if (!lid_closed)
		value = 1;

	return compare_condition(condition, value);
}

int cthd_engine_adaptive::evaluate_workload_condition(
		struct condition condition) {
	// We don't have a good way to assert workload at the moment, so just
	// default to bursty

	return compare_condition(condition, 3);
}

int cthd_engine_adaptive::evaluate_platform_type_condition(
		struct condition condition) {
	int value = 1;

	if (tablet_dev) {
		int tablet = libevdev_get_event_value(tablet_dev, EV_SW,
				SW_TABLET_MODE);
		if (tablet)
			value = 2;
	}
	return compare_condition(condition, value);
}

int cthd_engine_adaptive::evaluate_power_slider_condition(
		struct condition condition) {

	// We don't have a power slider currently, just set it to 75 which
	// equals "Better Performance" (using 100 would be more aggressive).

	return compare_condition(condition, 75);
}

int cthd_engine_adaptive::evaluate_ac_condition(struct condition condition) {
	int value = 0;
	bool on_battery = up_client_get_on_battery(upower_client);

	if (on_battery)
		value = 1;

	return compare_condition(condition, value);
}

int cthd_engine_adaptive::evaluate_condition(struct condition condition) {
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

	if (ret) {
		if (condition.time && condition.state_entry_time == 0) {
			condition.state_entry_time = time(NULL);
		}
		ret = compare_time(condition);
	} else {
		condition.state_entry_time = 0;
	}

	return ret;
}

int cthd_engine_adaptive::evaluate_condition_set(
		std::vector<struct condition> condition_set) {
	for (int i = 0; i < (int) condition_set.size(); i++) {
		thd_log_debug("evaluate condition.condition at index %d\n", i);
		if (evaluate_condition(condition_set[i]) != 0)
			return THD_ERROR;
	}
	return THD_SUCCESS;
}

int cthd_engine_adaptive::evaluate_conditions() {
	int target = -1;

	if (fallback_id >= 0)
		return -1;

	for (int i = 0; i < (int) conditions.size(); i++) {
		thd_log_debug("evaluate condition set %d\n", i);
		if (evaluate_condition_set(conditions[i]) == THD_SUCCESS) {
			if (policy_active && i == current_condition_set)
				break;

			current_condition_set = i;
			target = conditions[i][0].target;
			break;
		}
	}

	return target;
}

struct psvt* cthd_engine_adaptive::find_psvt(std::string name) {
	for (int i = 0; i < (int) psvts.size(); i++) {
		if (!strcasecmp(psvts[i].name.c_str(), name.c_str())) {
			return &psvts[i];
		}
	}

	return NULL;
}

int cthd_engine_adaptive::install_passive(struct psv *psv) {
	std::string psv_zone;

	size_t pos = psv->target.find_last_of(".");
	if (pos == std::string::npos)
		psv_zone = psv->target;
	else
		psv_zone = psv->target.substr(pos + 1);

	while (psv_zone.back() == '_') {
		psv_zone.pop_back();
	}

	cthd_zone *zone = search_zone(psv_zone);
	if (!zone) {
		if (!psv_zone.compare(0, 4, "B0D4")) {
			psv_zone= "TCPU";
			zone = search_zone(psv_zone);
		}

		if (!zone) {
			if (!psv_zone.compare(0, 4, "TCPU")) {
				psv_zone= "B0D4";
				zone = search_zone(psv_zone);
			}
			if (!zone) {
				thd_log_warn("Unable to find a zone for %s\n", psv_zone.c_str());
				return THD_ERROR;
			}
		}
	}

	std::string psv_cdev;
	pos = psv->source.find_last_of(".");
	if (pos == std::string::npos)
		psv_cdev = psv->source;
	else
		psv_cdev = psv->source.substr(pos + 1);

	while (psv_cdev.back() == '_') {
		psv_cdev.pop_back();
	}

	cthd_cdev *cdev = search_cdev(psv_cdev);

	if (!cdev) {
		if (!psv_cdev.compare(0, 4, "TCPU")) {
			psv_cdev= "B0D4";
			cdev = search_cdev(psv_cdev);
		}

		if (!cdev) {
			thd_log_warn("Unable to find a cooling device for %s\n",
				psv_cdev.c_str());
			return THD_ERROR;
		}
	}

	cthd_sensor *sensor = search_sensor(psv_zone);
	if (!sensor) {
		thd_log_warn("Unable to find a sensor for %s\n", psv_zone.c_str());
		return THD_ERROR;
	}

	int temp = (psv->temp - 2732) * 100;
	int target_state = 0;

	if (psv->limit.length()) {
		if (!strncasecmp(psv->limit.c_str(),"MAX", 3)) {
			target_state = TRIP_PT_INVALID_TARGET_STATE;
		} else if (!strncasecmp(psv->limit.c_str(),"MIN", 3)) {
			target_state = 0;
		} else {
			std::istringstream buffer(psv->limit);
			buffer >> target_state;
			target_state *= 1000;
		}
	}

	cthd_trip_point trip_pt(zone->get_trip_count(),
			PASSIVE,
			temp, 0,
			zone->get_zone_index(), sensor->get_index(),
			SEQUENTIAL);
	trip_pt.thd_trip_point_add_cdev(*cdev,
			cthd_trip_point::default_influence,
					psv->sample_period / 10,
					target_state ? 1 : 0,
					target_state,
					NULL);
	zone->add_trip(trip_pt, 1);
	zone->zone_cdev_set_binded();
	zone->set_zone_active();

	return 0;
}

void cthd_engine_adaptive::set_trip(std::string target, std::string argument) {
	std::string psv_zone;
	float float_temp = stof(argument, NULL);
	int temp = (int) (float_temp * 1000);

	size_t pos = target.find_last_of(".");

	if (pos == std::string::npos)
		psv_zone = target;
	else
		psv_zone = target.substr(pos + 1);

	while (psv_zone.back() == '_') {
		psv_zone.pop_back();
	}

	cthd_zone *zone = search_zone(psv_zone);
	if (!zone) {
		thd_log_warn("Unable to find a zone for %s\n", psv_zone.c_str());
		return;
	}

	int index = 0;
	cthd_trip_point *trip = zone->get_trip_at_index(index);
	while (trip != NULL) {
		if (trip->get_trip_type() == PASSIVE) {
			trip->update_trip_temp(temp);
			return;
		}
		index++;
		trip = zone->get_trip_at_index(index);
	}
	thd_log_warn("Unable to find a passive trippoint for %s\n", target.c_str());
}

void cthd_engine_adaptive::psvt_consolidate()
{
	/* Once all tables are installed, we need to consolidate since
	 * thermald has different implementation.
	 * If there is only entry of type MAX, then simply use thermald default at temperature + 1
	 * If there is a next trip after MAX for a target, then choose a temperature limit in the middle
	 */
	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *zone = zones[i];
		unsigned int count = zone->get_trip_count();

		for (unsigned int j = 0; j < count; ++j) {
			cthd_trip_point *trip = zone->get_trip_at_index(j);
			int target_state;
			thd_log_debug("check trip zone:%d:%d\n", i, j);
			if (trip->is_target_valid(target_state) == THD_SUCCESS) {

				if (target_state == TRIP_PT_INVALID_TARGET_STATE) {
					if (j == count - 1) {
						// This is the last "MAX" trip
						// So make the target state invalid and temperature + 1 C
						trip->set_first_target_invalid();
						trip->update_trip_temp(trip->get_trip_temp() + 1000);
					} else {
						// This is not the last trip. So something after this
						// if the next one has the same source and target
						cthd_trip_point *next_trip = zone->get_trip_at_index(
								j + 1);
						// Sinc this is not the last trip in this zone, we don't check
						// exception, next trip will be valid
						cthd_cdev *cdev = next_trip->get_first_cdev();
						if (!cdev) {
							// Something wrong make the current target invalid
							trip->set_first_target_invalid();
							trip->update_trip_temp(
									trip->get_trip_temp() + 1000);
							continue;
						}

						int next_target_state;

						if (trip->get_sensor_id()
								== next_trip->get_sensor_id() && trip->get_first_cdev()
								== next_trip->get_first_cdev() && next_trip->is_target_valid(next_target_state) == THD_SUCCESS) {

							// Same source and target and the target state of next is not of type MAX
							int state = cdev->get_min_state();
							target_state = (state + next_target_state) / 2;
							trip->set_first_target(target_state);
							trip->update_trip_temp(
									(next_trip->get_trip_temp()
											+ trip->get_trip_temp()) / 2);
						} else {
							// It has different source and target so
							// So make the target state invalid and temperature + 1 C
							trip->set_first_target_invalid();
							trip->update_trip_temp(
									trip->get_trip_temp() + 1000);
						}
					}
				}
			}
		}
	}
}

void cthd_engine_adaptive::set_int3400_target(struct adaptive_target target) {
	struct psvt *psvt;
	if (target.code == "PSVT") {
		thd_log_info("set_int3400 target %s\n", target.argument.c_str());

		psvt = find_psvt(target.argument);
		if (!psvt) {
			return;
		}

		for (unsigned int i = 0; i < zones.size(); ++i) {
			cthd_zone *_zone = zones[i];

			// This is only for debug to plot power, so keep
			if (_zone && _zone->get_zone_type() == "rapl_pkg_power")
				continue;

			_zone->zone_reset(1);
			_zone->trip_delete_all();

			if (_zone && _zone->zone_active_status())
				_zone->set_zone_inactive();
		}

		for (int i = 0; i < (int) psvt->psvs.size(); i++) {
			install_passive(&psvt->psvs[i]);
		}

		psvt_consolidate();

		thd_log_info("\n\n ZONE DUMP BEGIN\n");
		int new_zone_count = 0;
		for (unsigned int i = 0; i < zones.size(); ++i) {
			zones[i]->zone_dump();
			if (zones[i]->zone_active_status())
				++new_zone_count;
		}
		thd_log_info("\n\n ZONE DUMP END\n");
		if (!new_zone_count) {
			thd_log_warn("Adaptive policy couldn't create any zones\n");
			thd_log_warn("Possibly some sensors in the PSVT are missing\n");
			thd_log_warn("Restart in non adaptive mode via systemd\n");
			csys_fs sysfs("/tmp/ignore_adaptive");
			sysfs.create();
			exit(EXIT_FAILURE);
		}

	}
	if (target.code == "PSV") {
		set_trip(target.participant, target.argument);
	}
}

void cthd_engine_adaptive::execute_target(struct adaptive_target target) {
	cthd_cdev *cdev;
	std::string name;
	int argument;

	size_t pos = target.participant.find_last_of(".");
	if (pos == std::string::npos)
		name = target.participant;
	else
		name = target.participant.substr(pos + 1);
	cdev = search_cdev(name);
	if (!cdev) {
		if (!name.compare(0, 4, "TCPU")) {
			name = "B0D4";
			cdev = search_cdev(name);
		}
	}

	thd_log_info("looking for cdev %s\n", name.c_str());
	if (!cdev) {
		thd_log_info("cdev %s not found\n", name.c_str());
		if (target.participant == int3400_path) {
			set_int3400_target(target);
			return;
		}
	}

	if (target.code == "PSVT") {
		thd_log_info("PSVT...\n");
		set_int3400_target(target);
		return;
	}

	try {
		argument = std::stoi(target.argument, NULL);
	}
	catch (...) {
		thd_log_info("Invalid target target:%s %s\n", target.code.c_str(),
			target.argument.c_str());
		return;
	}
	thd_log_info("target:%s %d\n", target.code.c_str(), argument);
	if (cdev)
		cdev->set_adaptive_target(target);
}

void cthd_engine_adaptive::exec_fallback_target(int target){
	thd_log_debug("exec_fallback_target %d\n", target);
	for (int i = 0; i < (int) targets.size(); i++) {
		if (targets[i].target_id != (uint64_t) target)
			continue;
		execute_target(targets[i]);
	}
}

void cthd_engine_adaptive::update_engine_state() {

	if (passive_def_only) {
		if (!passive_def_processed) {
			thd_log_info("IETM_D0 processed\n");
			passive_def_processed = 1;

			for (unsigned int i = 0; i < zones.size(); ++i) {
				cthd_zone *_zone = zones[i];
				_zone->zone_reset(1);
				_zone->trip_delete_all();

				if (_zone->zone_active_status())
					_zone->set_zone_inactive();
			}

			struct psvt *psvt = find_def_psvt();
			if (!psvt)
				return;

			std::vector<struct psv> psvs = psvt->psvs;

			thd_log_info("Name :%s\n", psvt->name.c_str());
			for (unsigned int j = 0; j < psvs.size(); ++j) {
				install_passive(&psvs[j]);
			}
			psvt_consolidate();
			thd_log_info("\n\n ZONE DUMP BEGIN\n");
			for (unsigned int i = 0; i < zones.size(); ++i) {
				zones[i]->zone_dump();
			}
			thd_log_info("\n\n ZONE DUMP END\n");
		}

		return;
	}

	int target = evaluate_conditions();
	if (target == -1) {
		if (fallback_id >= 0 && !policy_active) {
			exec_fallback_target(targets[fallback_id].target_id);
			policy_active = 1;
		}
		return;
	}
	for (int i = 0; i < (int) targets.size(); i++) {
		if (targets[i].target_id != (uint64_t) target)
			continue;
		execute_target(targets[i]);
	}
	policy_active = 1;
}

int cthd_engine_adaptive::find_agressive_target() {
	int max_pl1_max = 0;
	int max_target_id = -1;

	for (int i = 0; i < (int) targets.size(); i++) {
		int argument;

		try {
			argument = std::stoi(targets[i].argument, NULL);
		}
		catch (...) {
			thd_log_info("Invalid target target:%s %s\n", targets[i].code.c_str(),
					targets[i].argument.c_str());
			continue;
		}
		thd_log_info("target:%s %d\n", targets[i].code.c_str(), argument);

		if (targets[i].code == "PL1MAX") {
			if (max_pl1_max < argument) {
				max_pl1_max = argument;
				max_target_id = i;
			}
		}
	}

	return max_target_id;
}

static int is_event_device(const struct dirent *dir) {
	return strncmp("event", dir->d_name, 5) == 0;
}

void cthd_engine_adaptive::setup_input_devices() {
	struct dirent **namelist;
	int i, ndev, ret;

	tablet_dev = NULL;

	ndev = scandir("/dev/input", &namelist, is_event_device, versionsort);
	for (i = 0; i < ndev; i++) {
		char fname[267];
		int fd = -1;

		snprintf(fname, sizeof(fname), "/dev/input/%s", namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd < 0)
			continue;
		ret = libevdev_new_from_fd(fd, &tablet_dev);
		if (ret) {
			close(fd);
			continue;
		}
		if (libevdev_has_event_code(tablet_dev, EV_SW, SW_TABLET_MODE))
			return;
		libevdev_free(tablet_dev);
		tablet_dev = NULL;
		close(fd);
	}
}

int cthd_engine_adaptive::thd_engine_start(bool ignore_cpuid_check, bool adaptive) {
	char *buf;
	csys_fs sysfs("");
	size_t size;

	if (!ignore_cpuid_check) {
		check_cpu_id();
		if (!processor_id_match()) {
			thd_log_msg("Unsupported cpu model or platform\n");
			exit(EXIT_SUCCESS);
		}
	}

	csys_fs _sysfs("/tmp/ignore_adaptive");
	if (_sysfs.exists()) {
		return THD_ERROR;
	}

	parser_disabled = true;
	force_mmio_rapl = true;

	if (sysfs.exists("/sys/bus/platform/devices/INT3400:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INT3400:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1040:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1040:00/";
	} else if (sysfs.exists("/sys/bus/platform/devices/INTC1041:00")) {
		int3400_base_path = "/sys/bus/platform/devices/INTC1041:00/";
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

	try {
		if (parse_gddv(buf, size, NULL)) {
			thd_log_debug("Unable to parse GDDV");
			delete[] buf;
			return THD_FATAL_ERROR;
		}

		merge_appc();

		dump_ppcc();
		dump_psvt();
		dump_apat();
		dump_apct();
	} catch (std::exception &e) {
		thd_log_warn("%s\n", e.what());
		delete [] buf;
		return THD_FATAL_ERROR;
	}

	if (!conditions.size()) {
		thd_log_info("No adaptive conditions present\n");

		struct psvt *psvt = find_def_psvt();

		if (psvt) {
			thd_log_info("IETM.D0 found\n");
			passive_def_only = 1;
		}
		return cthd_engine::thd_engine_start(ignore_cpuid_check);
	}

	setup_input_devices();

	upower_client = up_client_new();
	if (!upower_client) {
		thd_log_info("Unable to connect to upower\n");
	}

	if (verify_conditions()) {
		thd_log_info("Some conditions are not supported, so check if any condition set can be matched\n");
		int target = evaluate_conditions();
		if (target == -1) {
			thd_log_info("Also unable to evaluate any conditions\n");
			thd_log_info("Falling back to use configuration with the highest power\n");

			if (tablet_dev)
				libevdev_free(tablet_dev);

			// Looks like there is no free call for up_client_new()

			int i = find_agressive_target();
			thd_log_info("target:%d\n", i);
			if (i >= 0) {
				thd_log_info("fallback id:%d\n", i);
				fallback_id = i;
			} else {
				delete[] buf;
				return THD_ERROR;
			}
		}
	}

	set_control_mode(EXCLUSIVE);

	evaluate_conditions();
	thd_log_info("adaptive engine reached end");

	return cthd_engine::thd_engine_start(ignore_cpuid_check, adaptive);
}

int thd_engine_create_adaptive_engine(bool ignore_cpuid_check) {
	thd_engine = new cthd_engine_adaptive();

	thd_engine->set_poll_interval(thd_poll_interval);

	// Initialize thermald objects
	if (thd_engine->thd_engine_start(ignore_cpuid_check, true) != THD_SUCCESS) {
		thd_log_info("THD engine start failed\n");
		return THD_ERROR;
	}

	return THD_SUCCESS;
}
