#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <lzma.h>
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

struct header {
	uint16_t signature;
        uint16_t headersize;
        uint32_t version;
        uint32_t flags;
} __attribute__ ((packed));

cthd_engine_adaptive::~cthd_engine_adaptive() {
}

int cthd_engine_adaptive::get_type(char *object, int *offset) {
	return *(uint32_t *)(object + *offset);
}

uint64_t cthd_engine_adaptive::get_uint64(char *object, int *offset) {
        uint64_t value;
        int type = *(uint32_t *)(object + *offset);

        if (type != 4) {
		thd_log_fatal("Found object of type %d, expecting 4\n", type);
        }
        *offset += 4;

        value = *(uint64_t *)(object + *offset);
        *offset += 8;

        return value;
}

char * cthd_engine_adaptive::get_string(char *object, int *offset) {
        int type = *(uint32_t *)(object + *offset);
        uint64_t length;
        char *value;

        if (type != 8) {
		thd_log_fatal("Found object of type %d, expecting 8\n", type);
        }
        *offset += 4;

        length = *(uint64_t *)(object + *offset);
        *offset += 8;

        value = &object[*offset];
        *offset += length;
        return value;
}

int cthd_engine_adaptive::parse_appc (char *appc, int len) {
	/* Currently unhandled */
        return 0;
}

int cthd_engine_adaptive::parse_apat (char *apat, int len) {
        int offset = 0;
        uint64_t version = get_uint64(apat, &offset);

	if (version != 2)
		thd_log_fatal("Found unsupported APAT version %d\n", (int)version);

        while (1) {
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

int cthd_engine_adaptive::parse_apct (char *apct, int len) {
        int i;
        int offset = 0;
        uint64_t version = get_uint64(apct, &offset);

        if (version == 1) {
                while (offset < len) {
			std::vector<struct condition> condition_set;

			uint64_t target = get_uint64(apct, &offset);
			if (int(target) == -1) {
				thd_log_fatal("Invalid APCT target\n");
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
					thd_log_fatal("Read off end of buffer in APCT parsing\n");
                                }

				condition.condition = adaptive_condition(get_uint64(apct, &offset));
				condition.comparison = adaptive_comparison(get_uint64(apct, &offset));
				condition.argument = get_uint64(apct, &offset);
				if (i < 9) {
					condition.operation = adaptive_operation(get_uint64(apct, &offset));
					if (condition.operation == FOR) {
						offset += 8;
						condition.time_comparison = adaptive_comparison(get_uint64(apct, &offset));
						condition.time = get_uint64(apct, &offset);
						offset += 8;
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
				thd_log_fatal("Invalid APCT target");
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
					thd_log_fatal("Read off end of buffer in parsing APCT\n");
                                }

				condition.condition = adaptive_condition(get_uint64(apct, &offset));
				condition.device = get_string(apct, &offset);
				offset += 12;
				condition.comparison = adaptive_comparison(get_uint64(apct, &offset));
				condition.argument = get_uint64(apct, &offset);
				if (i < int(count - 1)) {
					condition.operation = adaptive_operation(get_uint64(apct, &offset));
					if (condition.operation == FOR) {
						offset += 12;
						get_string(apct, &offset);
						offset += 12;
						condition.time_comparison = adaptive_comparison(get_uint64(apct, &offset));
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
		thd_log_fatal("Unsupported APCT version %d\n", (int)version);
	}
        return 0;
}

ppcc_t * cthd_engine_adaptive::get_ppcc_param(std::string name) {
	for (int i = 0; i < (int)ppccs.size(); i++) {
		if (ppccs[i].name == name)
			return &ppccs[i];
	}

	return NULL;
}

int cthd_engine_adaptive::parse_ppcc (char *name, char *buf, int len) {
	ppcc_t ppcc;

	ppcc.name = name;
        ppcc.power_limit_min = *(uint64_t *)(buf + 28);
        ppcc.power_limit_max = *(uint64_t *)(buf + 40);
        ppcc.time_wind_min = *(uint64_t *)(buf + 52);
        ppcc.time_wind_max = *(uint64_t *)(buf + 64);
        ppcc.step_size = *(uint64_t *)(buf + 76);

	ppccs.push_back(ppcc);

        return 0;
}

int cthd_engine_adaptive::parse_psvt(char *name, char *buf, int len) {
	int offset = 0;
	int version = get_uint64(buf, &offset);
	struct psvt psvt;

	if (version != 2)
		thd_log_fatal("Found unsupported PSVT version %d\n", (int)version);

	if (name == NULL)
		psvt.name = "Default";
	else
		psvt.name = name;
	psvt.source = get_string(buf, &offset);
	psvt.target = get_string(buf, &offset);
	psvt.priority = get_uint64(buf, &offset);
	psvt.sample_period = get_uint64(buf, &offset);
	psvt.temp = get_uint64(buf, &offset);
	psvt.domain = get_uint64(buf, &offset);
	psvt.control_knob = get_uint64(buf, &offset);
	if (get_type(buf, &offset) == 8) {
		psvt.limit = get_string(buf, &offset);
	} else {
		uint64_t tmp = get_uint64(buf, &offset);
		psvt.limit = std::to_string(tmp);
	}
	psvt.step_size = get_uint64(buf, &offset);
	psvt.limit_coeff = get_uint64(buf, &offset);
	psvt.unlimit_coeff = get_uint64(buf, &offset);

	psvts.push_back(psvt);

	return 0;
}

int cthd_engine_adaptive::handle_compressed_gddv(char *buf, int size) {
	uint64_t output_size = *(uint64_t *)(buf+5);
	lzma_ret ret;
	unsigned char *decompressed = (unsigned char*)malloc(output_size);
	lzma_stream strm = LZMA_STREAM_INIT;

	if (!decompressed)
		thd_log_fatal("Failed to allocate buffer for decompressed output\n");
	ret = lzma_auto_decoder(&strm, 64 * 1024 * 1024, 0);
	if (ret)
		thd_log_fatal("Failed to initialize LZMA decoder: %d\n", ret);

	strm.next_out = decompressed;
	strm.avail_out = output_size;
	strm.next_in = (const unsigned char *)(buf);
	strm.avail_in = size;
	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);
	if (ret && ret != LZMA_STREAM_END)
		thd_log_fatal("Failed to decompress GDDV data: %d\n", ret);

	parse_gddv((char *)decompressed, output_size);
	free(decompressed);

	return THD_SUCCESS;
}

int cthd_engine_adaptive::parse_gddv(char *buf, int size) {
	int offset = 0;
	struct header *header;

	header = (struct header *)buf;

	if (header->signature != 0x1fe5)
		thd_log_fatal("Unexpected GDDV signature 0x%x\n", header->signature);

	if (header->version != htonl(1) && header->version != htonl(2))
		return THD_ERROR;

	offset = header->headersize;

	while (offset < size) {
		uint16_t unk1;
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

		if (header->version == htonl(2)) {
			memcpy(&unk1, buf + offset, sizeof(unk1));
			if (unk1 == 0x005d) {
				return handle_compressed_gddv(buf + offset, size - offset);
			}
			offset += sizeof(unk1);
		}

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

		str = strtok(key, "/");
		if (!str) {
			free(key);
			free(val);
			continue;
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
		if (type && strcmp(type, "ppcc") == 0) {
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

		delete(key);
		delete(val);
	}

	return 0;
}

int cthd_engine_adaptive::verify_condition(struct condition condition) {
	if (condition.condition >= Oem0 &&
	    condition.condition <= Oem5)
		return 0;
	if (condition.condition >= adaptive_condition(0x1000) &&
	    condition.condition < adaptive_condition(0x10000))
		return 0;
	if (condition.condition == Default)
		return 0;

	thd_log_fatal("Unsupported condition %d, exiting\n", condition.condition);
	return THD_ERROR;
}

int cthd_engine_adaptive::verify_conditions() {
	for (int i = 0; i < (int)conditions.size(); i++) {
		for (int j = 0; j < (int)conditions[i].size(); j++) {
			if (verify_condition(conditions[i][j]))
				return THD_ERROR;
		}
	}
	return 0;
}

int cthd_engine_adaptive::evaluate_condition(struct condition condition) {
	int oem_condition = -1;
	csys_fs sysfs("/sys/bus/platform/devices/INT3400:00/");

	if (condition.condition == Default)
		return THD_SUCCESS;

	if (condition.condition >= Oem0 &&
	    condition.condition <= Oem5)
		oem_condition = (int)condition.condition - 19;
	else if (condition.condition >= (adaptive_condition)0x1000 &&
		 condition.condition < (adaptive_condition)0x10000)
		oem_condition = (int)condition.condition - 0x1000 + 6;

	if (oem_condition != -1) {
		std::string filename = "odvp" + std::to_string(oem_condition);
		std::string data;
		if (sysfs.read(filename, data) < 0) {
			thd_log_error("Unable to read %s\n", filename.c_str());
			return THD_ERROR;
		}
		int value = std::stoi(data, NULL);

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
		}
	}

	return THD_ERROR;
}

int cthd_engine_adaptive::evaluate_condition_set(std::vector<struct condition> condition_set) {
	for (int i = 0; i < (int)condition_set.size(); i++) {
		if (evaluate_condition(condition_set[i]) != 0)
			return THD_ERROR;
	}
	return THD_SUCCESS;
}
	
int cthd_engine_adaptive::evaluate_conditions() {
	int target = -1;

	for (int i = 0; i < (int)conditions.size(); i++) {
		if (evaluate_condition_set(conditions[i]) == THD_SUCCESS) {
			target = conditions[i][0].target;
			break;
		}
	}

	return target;
}

struct psvt *cthd_engine_adaptive::find_psvt(std::string name) {
	for (int i = 0; i < (int)psvts.size(); i++) {
		if (psvts[i].name == name) {
			return &psvts[i];
		}
	}

	return NULL;
}

void cthd_engine_adaptive::set_int3400_target(struct adaptive_target target) {
	struct psvt *psvt;
	if (target.code == "PSVT") {
		psvt = find_psvt(target.argument);
		if (!psvt) {
			return;
		}
		std::string psv_zone;
		size_t pos = psvt->target.find_last_of(".");
		if (pos == std::string::npos)
			psv_zone = psvt->target;
		else
			psv_zone = psvt->target.substr(pos + 1);

		cthd_zone *zone = search_zone(psv_zone);
		if (!zone)
			return;

		std::string psv_cdev;
		pos = psvt->source.find_last_of(".");
		if (pos == std::string::npos)
			psv_cdev = psvt->source;
		else
			psv_cdev = psvt->source.substr(pos + 1);

		cthd_cdev *cdev = search_cdev(psv_cdev);

		// HACK - if the cdev is RAPL and the sensor is memory, use
		// the rapl-dram device instead
		if (psv_zone == "TMEM" && psv_cdev == "B0D4")
			cdev = search_cdev("rapl_controller_dram");

		if (!cdev)
			return;

		cthd_sensor *sensor = search_sensor(psv_zone);
		if (!sensor)
			return;

		int temp = (psvt->temp - 2732) * 100;
		int index = 0;
		cthd_trip_point *trip = zone->get_trip_at_index(index);
		while (trip != NULL) {
			trip->trip_dump();
			if (trip->get_trip_type() == PASSIVE) {
				int cdev_index = 0;
				while (true) {
					try {
						trip_pt_cdev_t trip_cdev = trip->get_cdev_at_index(cdev_index);
						if (trip_cdev.cdev == cdev) {
							trip->update_trip_temp(temp);
							return;
						}
						cdev_index++;
					} catch (const std::invalid_argument &) {
						break;
					}
				}
			}
			index++;
			trip = zone->get_trip_at_index(index);
		}

		// If we're here, there's no existing trip point for this
		// device that includes the relevant cdev. Add one.
		cthd_trip_point trip_pt(index, PASSIVE, temp, 0,
					zone->get_zone_index(),
					sensor->get_index());
		trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence, psvt->sample_period/10);
		zone->add_trip(trip_pt);
		zone->zone_cdev_set_binded();
	}
}

void cthd_engine_adaptive::execute_target(struct adaptive_target target) {
	cthd_cdev *cdev;
	std::string name;

	size_t pos = target.participant.find_last_of(".");
	if (pos == std::string::npos)
		name = target.participant;
	else
		name = target.participant.substr(pos + 1);
	cdev = search_cdev(name);
	if (!cdev) {
		if (target.participant == int3400_path) {
			set_int3400_target(target);
		}
		return;
	}
	cdev->set_adaptive_target(target);
}

void cthd_engine_adaptive::update_engine_state() {
	int target = evaluate_conditions();
	if (target == -1)
		return;
	for (int i = 0; i < (int)targets.size(); i++) {
		if (targets[i].target_id != (uint64_t)target)
			continue;
		execute_target(targets[i]);
	}
}

int cthd_engine_adaptive::thd_engine_start(bool ignore_cpuid_check) {
	char *buf;
	csys_fs sysfs("/sys/");
	size_t size;

	parser_disabled = true;
	force_mmio_rapl = true;

	if (sysfs.read("bus/platform/devices/INT3400:00/firmware_node/path",
		       int3400_path) < 0) {
		thd_log_error("Unable to locate INT3400 firmware path\n");
		return THD_ERROR;
	}

	size = sysfs.size("bus/platform/devices/INT3400:00/data_vault");
	if (size == 0) {
                thd_log_error("Unable to open GDDV data vault\n");
		return THD_ERROR;
	}

	buf = new char[size];
	if (!buf) {
		thd_log_error("Unable to allocate memory for GDDV");
		return THD_ERROR;
	}

	if (sysfs.read("bus/platform/devices/INT3400:00/data_vault", buf, size) < int(size)) {
		thd_log_error("Unable to read GDDV data vault\n");
		return THD_ERROR;
	}

	if (parse_gddv(buf, size)) {
		thd_log_error("Unable to parse GDDV");
		return THD_ERROR;
	}

	if (verify_conditions()) {
		thd_log_error("Unable to verify conditions are supported\n");
		return THD_ERROR;
	}

	set_control_mode(EXCLUSIVE);

	evaluate_conditions();

	return cthd_engine::thd_engine_start(ignore_cpuid_check);
}

int thd_engine_create_adaptive_engine(bool ignore_cpuid_check) {
        thd_engine = new cthd_engine_adaptive();

        thd_engine->set_poll_interval(thd_poll_interval);

        // Initialize thermald objects
        if (thd_engine->thd_engine_start(ignore_cpuid_check) != THD_SUCCESS) {
                thd_log_error("THD engine start failed\n");
		return THD_ERROR;
	}

        return THD_SUCCESS;
}
