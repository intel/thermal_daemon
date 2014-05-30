/*
 * cthd_engine_defualt.cpp: Default thermal engine
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include "thd_engine_default.h"
#include "thd_zone_cpu.h"
#include "thd_zone_generic.h"
#include "thd_cdev_gen_sysfs.h"
#include "thd_cdev_cpufreq.h"
#include "thd_cdev_rapl.h"
#include "thd_cdev_intel_pstate_driver.h"
#include "thd_zone_surface.h"
#include "thd_cdev_msr_rapl.h"
#include "thd_cdev_rapl_dram.h"

#define ACTIVATE_SURFACE

// Default CPU cooling devices, which are not part of thermal sysfs
// Since non trivial initialization is not supported, we init all fields even if they are not needed
/* Some security scan handler can't parse, the following block and generate unnecessary errors.
 * hiding good ones. So init in old style compatible to C++

 static cooling_dev_t cpu_def_cooling_devices[] = {
 {	.status = true,
 .mask = CDEV_DEF_BIT_UNIT_VAL | CDEV_DEF_BIT_READ_BACK | CDEV_DEF_BIT_MIN_STATE | CDEV_DEF_BIT_STEP,
 .index = 0, .unit_val = ABSOULUTE_VALUE, .min_state = 0, .max_state = 0, .inc_dec_step = 5,
 .read_back = false, .auto_down_control = false,
 .type_string = "intel_powerclamp", .path_str = "",
 .debounce_interval = 4, .pid_enable = false,
 .pid = {0.0, 0.0, 0.0}},
 };
 */
static cooling_dev_t cpu_def_cooling_devices[] = { { true, CDEV_DEF_BIT_UNIT_VAL
		| CDEV_DEF_BIT_READ_BACK | CDEV_DEF_BIT_MIN_STATE | CDEV_DEF_BIT_STEP,
		0, ABSOULUTE_VALUE, 0, 0, 5, false, false, "intel_powerclamp", "", 4,
		false, { 0.0, 0.0, 0.0 } }, { true, CDEV_DEF_BIT_UNIT_VAL
		| CDEV_DEF_BIT_READ_BACK | CDEV_DEF_BIT_MIN_STATE | CDEV_DEF_BIT_STEP,
		0, ABSOULUTE_VALUE, 0, 100, 5, false, false, "LCD", "", 4, false, { 0.0,
				0.0, 0.0 } } };

cthd_engine_default::~cthd_engine_default() {
	if (parser_init_done)
		parser.parser_deinit();
}

int cthd_engine_default::parser_init() {
	if (parser_init_done)
		return THD_SUCCESS;
	if (parser.parser_init() == THD_SUCCESS) {
		if (parser.start_parse() == THD_SUCCESS) {
			parser.dump_thermal_conf();
			parser_init_done = true;
			return THD_SUCCESS;
		}
	}

	return THD_ERROR;
}

void cthd_engine_default::parser_deinit() {
	if (parser_init_done) {
		parser.parser_deinit();
		parser_init_done = false;
	}
}

int cthd_engine_default::read_thermal_sensors() {
	int index;
	DIR *dir;
	struct dirent *entry;
	int sensor_mask = 0x0f;
	cthd_sensor *sensor;
	const std::string base_path[] = { "/sys/devices/platform/",
			"/sys/class/hwmon/" };
	int i;

	thd_read_default_thermal_sensors();
	index = sensor_count;

	sensor = search_sensor("pkg-temp-0");
	if (sensor) {
		// Force this to support async
		sensor->set_async_capable(true);
	}

	sensor = search_sensor("x86_pkg_temp");
	if (sensor) {
		// Force this to support async
		sensor->set_async_capable(true);
	}

	sensor = search_sensor("soc_dts0");
	if (sensor) {
		// Force this to support async
		sensor->set_async_capable(true);
	}

	// Default CPU temperature zone
	// Find path to read DTS temperature
	for (i = 0; i < 2; ++i) {
		if ((dir = opendir(base_path[i].c_str())) != NULL) {
			while ((entry = readdir(dir)) != NULL) {
				if (!strncmp(entry->d_name, "coretemp.", strlen("coretemp."))
						|| !strncmp(entry->d_name, "hwmon", strlen("hwmon"))) {

					// Check name
					std::string name_path = base_path[i] + entry->d_name
							+ "/name";
					csys_fs name_sysfs(name_path.c_str());
					if (!name_sysfs.exists()) {
						thd_log_info("dts %s doesn't exist\n",
								name_path.c_str());
						continue;
					}
					std::string name;
					if (name_sysfs.read("", name) < 0) {
						thd_log_info("dts name read failed for %s\n",
								name_path.c_str());
						continue;
					}
					if (name != "coretemp")
						continue;

					int cnt = 0;
					unsigned int mask = 0x1;
					do {
						if (sensor_mask & mask) {
							std::stringstream temp_input_str;
							std::string path = base_path[i] + entry->d_name
									+ "/";
							csys_fs dts_sysfs(path.c_str());
							temp_input_str << "temp" << cnt << "_input";
							if (dts_sysfs.exists(temp_input_str.str())) {
								cthd_sensor *sensor = new cthd_sensor(index,
										base_path[i] + entry->d_name + "/"
												+ temp_input_str.str(), "hwmon",
										SENSOR_TYPE_RAW);
								if (sensor->sensor_update() != THD_SUCCESS) {
									delete sensor;
									closedir(dir);
									return THD_ERROR;
								}
								sensors.push_back(sensor);
								++index;
							}
						}
						mask = (mask << 1);
						cnt++;
					} while (mask != 0);
				}
			}
			closedir(dir);
		}
		if (index != sensor_count)
			break;
	}
	if (index == sensor_count) {
		// No coretemp sysfs exist, try hwmon
		thd_log_warn("Thermal DTS: No coretemp sysfs, trying hwmon \n");
		cthd_sensor *sensor = new cthd_sensor(index,
				"/sys/class/hwmon/hwmon0/temp1_input", "hwmon",
				SENSOR_TYPE_RAW);
		if (sensor->sensor_update() != THD_SUCCESS) {
			delete sensor;
			return THD_ERROR;
		}
		sensors.push_back(sensor);
		++index;

		if (index == sensor_count) {
			thd_log_error("Thermal DTS or hwmon: No Zones present: \n");
			return THD_FATAL_ERROR;
		}
	}

	// Add from XML sensor config
	if (!parser_init() && parser.platform_matched()) {
		for (int i = 0; i < parser.sensor_count(); ++i) {
			thermal_sensor_t *sensor_config = parser.get_sensor_dev_index(i);
			if (!sensor_config)
				continue;
			cthd_sensor *sensor = search_sensor(sensor_config->name);
			if (sensor) {
				if (sensor_config->mask & SENSOR_DEF_BIT_PATH)
					sensor->update_path(sensor_config->path);
				if (sensor_config->mask & SENSOR_DEF_BIT_ASYNC_CAPABLE)
					sensor->set_async_capable(sensor_config->async_capable);
			} else {
				cthd_sensor *sensor_new = new cthd_sensor(index,
						sensor_config->path, sensor_config->name,
						SENSOR_TYPE_RAW);
				if (sensor_new->sensor_update() != THD_SUCCESS) {
					delete sensor_new;
					continue;
				}
				sensors.push_back(sensor_new);
				++index;
			}
		}
		sensor_count = index;
	}

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		sensors[i]->sensor_dump();
	}

	return THD_SUCCESS;
}

int cthd_engine_default::read_thermal_zones() {
	int count = 0;
	DIR *dir;
	struct dirent *entry;
	const std::string base_path[] = { "/sys/devices/platform/",
			"/sys/class/hwmon/" };
	int i;

	thd_read_default_thermal_zones();
	count = zone_count;

	if (!search_zone("cpu")) {
		bool cpu_zone_created = false;
		thd_log_info("zone cpu will be created \n");
		// Default CPU temperature zone
		// Find path to read DTS temperature
		for (i = 0; i < 2; ++i) {
			if ((dir = opendir(base_path[i].c_str())) != NULL) {
				while ((entry = readdir(dir)) != NULL) {
					if (!strncmp(entry->d_name, "coretemp.",
							strlen("coretemp."))
							|| !strncmp(entry->d_name, "hwmon",
									strlen("hwmon"))) {

						std::string name_path = base_path[i] + entry->d_name
								+ "/name";
						csys_fs name_sysfs(name_path.c_str());
						if (!name_sysfs.exists()) {
							thd_log_info("dts zone %s doesn't exist\n",
									name_path.c_str());
							continue;
						}
						std::string name;
						if (name_sysfs.read("", name) < 0) {
							thd_log_info("dts zone name read failed for %s\n",
									name_path.c_str());
							continue;
						}
						thd_log_info("%s->%s\n", name_path.c_str(),
								name.c_str());
						if (name != "coretemp")
							continue;

						cthd_zone_cpu *zone = new cthd_zone_cpu(count,
								base_path[i] + entry->d_name + "/",
								atoi(entry->d_name + strlen("coretemp.")));
						if (zone->zone_update() == THD_SUCCESS) {
							zone->set_zone_active();
							zones.push_back(zone);
							++count;
							cpu_zone_created = true;
						} else {
							delete zone;
						}
					}
				}
				closedir(dir);
			}
			if (cpu_zone_created)
				break;
		}

		if (!cpu_zone_created) {
			// No coretemp sysfs exist, try hwmon
			thd_log_warn("Thermal DTS: No coretemp sysfs, trying hwmon \n");

			cthd_zone_cpu *zone = new cthd_zone_cpu(count,
					"/sys/class/hwmon/hwmon0/", 0);
			if (zone->zone_update() == THD_SUCCESS) {
				zone->set_zone_active();
				zones.push_back(zone);
				++count;
				cpu_zone_created = true;
			} else {
				delete zone;
			}

			if (!cpu_zone_created) {
				thd_log_error("Thermal DTS or hwmon: No Zones present: \n");
				return THD_FATAL_ERROR;
			}
		}
	}
	zone_count = count;

	// Add from XML thermal zone
	if (!parser_init() && parser.platform_matched()) {
		for (int i = 0; i < parser.zone_count(); ++i) {
			bool activate;
			thermal_zone_t *zone_config = parser.get_zone_dev_index(i);
			if (!zone_config)
				continue;
			thd_log_debug("Look for Zone  [%s] \n", zone_config->type.c_str());
			cthd_zone *zone = search_zone(zone_config->type);
			if (zone) {
				activate = false;
				thd_log_info("Zone already present %s \n",
						zone_config->type.c_str());
				for (unsigned int k = 0; k < zone_config->trip_pts.size();
						++k) {
					trip_point_t &trip_pt_config = zone_config->trip_pts[k];
					thd_log_debug(
							"Trip %d, temperature %d, Look for Search sensor %s\n",
							k, trip_pt_config.temperature,
							trip_pt_config.sensor_type.c_str());
					cthd_sensor *sensor = search_sensor(
							trip_pt_config.sensor_type);
					if (!sensor) {
						thd_log_error("XML zone: invalid sensor type [%s]\n",
								trip_pt_config.sensor_type.c_str());
						// This will update the trip temperature for the matching
						// trip type
						if (trip_pt_config.temperature) {
							cthd_trip_point trip_pt(zone->get_trip_count(),
									trip_pt_config.trip_pt_type,
									trip_pt_config.temperature,
									trip_pt_config.hyst, zone->get_zone_index(),
									-1, trip_pt_config.control_type);
							zone->update_trip_temp(trip_pt);
						}
						continue;
					}
					zone->bind_sensor(sensor);
					if (trip_pt_config.temperature) {

						cthd_trip_point trip_pt(zone->get_trip_count(),
								trip_pt_config.trip_pt_type,
								trip_pt_config.temperature, trip_pt_config.hyst,
								zone->get_zone_index(), sensor->get_index(),
								trip_pt_config.control_type);
						// bind cdev
						for (unsigned int j = 0;
								j < trip_pt_config.cdev_trips.size(); ++j) {
							cthd_cdev *cdev = search_cdev(
									trip_pt_config.cdev_trips[j].type);
							if (cdev) {
								trip_pt.thd_trip_point_add_cdev(*cdev,
										trip_pt_config.cdev_trips[j].influence,
										trip_pt_config.cdev_trips[j].sampling_period);
								zone->zone_cdev_set_binded();
								activate = true;
							}
						}
						zone->add_trip(trip_pt);
					} else {
						thd_log_debug("Trip temp == 0 is in zone %s \n",
								zone_config->type.c_str());
						// Try to find some existing non zero trips and associate the cdevs
						// This is the way from an XML config a generic cooling device
						// can be bound. For example from ACPI thermal relationships tables
						for (unsigned int j = 0;
								j < trip_pt_config.cdev_trips.size(); ++j) {
							cthd_cdev *cdev = search_cdev(
									trip_pt_config.cdev_trips[j].type);
							if (!cdev) {
								thd_log_info("cdev for type %s not found\n",
										trip_pt_config.cdev_trips[j].type.c_str());
							}
							if (cdev) {
								if (zone->bind_cooling_device(
										trip_pt_config.trip_pt_type, 0, cdev,
										trip_pt_config.cdev_trips[j].influence) == THD_SUCCESS) {
									thd_log_debug(
											"bind %s to trip to sensor %s\n",
											cdev->get_cdev_type().c_str(),
											sensor->get_sensor_type().c_str());
									activate = true;
								} else {
									thd_log_debug(
											"bind_cooling_device failed for cdev %s trip %s\n",
											cdev->get_cdev_type().c_str(),
											sensor->get_sensor_type().c_str());
								}

							}
						}
					}
				}
				if (activate) {
					thd_log_debug("Activate zone %s\n",
							zone->get_zone_type().c_str());
					zone->set_zone_active();
				}
			} else {
				cthd_zone_generic *zone = new cthd_zone_generic(count, i,
						zone_config->type);
				if (zone->zone_update() == THD_SUCCESS) {
					zones.push_back(zone);
					++count;
					zone->set_zone_active();
				} else
					delete zone;
			}
		}
	}
	zone_count = count;

#ifdef ACTIVATE_SURFACE
//	Enable when skin sensors are standardized
	cthd_zone *surface;
	surface = search_zone("Surface");

	if (!surface || (surface && !surface->zone_active_status())) {
		cthd_zone_surface *zone = new cthd_zone_surface(count);
		if (zone->zone_update() == THD_SUCCESS) {
			zones.push_back(zone);
			++count;
			zone->set_zone_active();
		} else
			delete zone;
	} else {
		thd_log_info("TSKN sensor was activated by config \n");
	}
#endif
	zone_count = count;

	def_binding.do_default_binding(cdevs);

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zones[i]->zone_dump();
	}

	return THD_SUCCESS;
}

int cthd_engine_default::add_replace_cdev(cooling_dev_t *config) {
	cthd_cdev *cdev;
	bool cdev_present = false;
	int current_cdev_index = cdev_cnt;
	bool percent_unit = false;

	// Check if there is existing cdev with this name and path
	cdev = search_cdev(config->type_string);
	if (cdev) {
		cdev_present = true;
		// Also check for path, some device like FAN has multiple paths for same type_str
		std::string base_path = cdev->get_base_path();
		if (config->path_str.size() && config->path_str != base_path) {
			cdev_present = false;
		}
	}
	if (!cdev_present) {
		// create new
		cdev = new cthd_gen_sysfs_cdev(current_cdev_index, config->path_str);
		if (!cdev)
			return THD_ERROR;
		cdev->set_cdev_type(config->type_string);
		if (cdev->update() != THD_SUCCESS) {
			delete cdev;
			return THD_ERROR;
		}
		cdevs.push_back(cdev);
	}

	if (config->mask & CDEV_DEF_BIT_UNIT_VAL) {
		if (config->unit_val == RELATIVE_PERCENTAGES)
			percent_unit = true;
	}
	if (config->mask & CDEV_DEF_BIT_AUTO_DOWN)
		cdev->set_down_adjust_control(config->auto_down_control);
	if (config->mask & CDEV_DEF_BIT_STEP) {
		if (percent_unit)
			cdev->set_inc_dec_value(
					cdev->get_curr_state() * config->inc_dec_step / 100);
		else
			cdev->set_inc_dec_value(config->inc_dec_step);
	}
	if (config->mask & CDEV_DEF_BIT_MIN_STATE) {
		if (percent_unit)
			cdev->thd_cdev_set_min_state_param(
					cdev->get_curr_state() * config->min_state / 100);
		else
			cdev->thd_cdev_set_min_state_param(config->min_state);
	}
	if (config->mask & CDEV_DEF_BIT_MAX_STATE) {
		if (percent_unit)
			cdev->thd_cdev_set_max_state_param(
					cdev->get_curr_state() * config->max_state / 100);
		else
			cdev->thd_cdev_set_max_state_param(config->max_state);
	}
	if (config->mask & CDEV_DEF_BIT_READ_BACK)
		cdev->thd_cdev_set_read_back_param(config->read_back);

	if (config->mask & CDEV_DEF_BIT_DEBOUNCE_VAL)
		cdev->set_debounce_interval(config->debounce_interval);

	if (config->mask & CDEV_DEF_BIT_PID_PARAMS) {
		cdev->enable_pid();
		cdev->set_pid_param(config->pid.Kp, config->pid.Ki, config->pid.Kd);
	}
	//cdev->cdev_dump();

	current_cdev_index++;

	return THD_SUCCESS;

}

int cthd_engine_default::read_cooling_devices() {
	int size;
	int i;

	// Read first all the default cooling devices added by kernel
	thd_read_default_cooling_devices();
	int current_cdev_index = cdev_cnt;

	// Add RAPL cooling device
	cthd_sysfs_cdev_rapl *rapl_dev = new cthd_sysfs_cdev_rapl(cdev_cnt, 0);
	rapl_dev->set_cdev_type("rapl_controller");
	if (rapl_dev->update() == THD_SUCCESS) {
		cdevs.push_back(rapl_dev);
		++cdev_cnt;
	} else {
		delete rapl_dev;
		if (processor_id_match()) {
			// RAPL control via MSR
			cthd_cdev_rapl_msr *rapl_msr_dev = new cthd_cdev_rapl_msr(cdev_cnt,
					0);
			rapl_msr_dev->set_cdev_type("rapl_controller");
			if (rapl_msr_dev->update() == THD_SUCCESS) {
				cdevs.push_back(rapl_msr_dev);
				++cdev_cnt;
			} else
				delete rapl_msr_dev;
		}
	}
	// Add Intel P state driver as cdev
	cthd_intel_p_state_cdev *pstate_dev = new cthd_intel_p_state_cdev(cdev_cnt);
	pstate_dev->set_cdev_type("intel_pstate");
	if (pstate_dev->update() == THD_SUCCESS) {
		cdevs.push_back(pstate_dev);
		++cdev_cnt;
	} else
		delete pstate_dev;

	// Add statically defined cooling devices
	size = sizeof(cpu_def_cooling_devices) / sizeof(cooling_dev_t);
	for (i = 0; i < size; ++i) {
		if (add_replace_cdev(&cpu_def_cooling_devices[i]) == THD_SUCCESS)
			current_cdev_index++;
		cdev_cnt = current_cdev_index;
	}

	// Add from XML cooling device config
	if (!parser_init() && parser.platform_matched()) {
		for (int i = 0; i < parser.cdev_count(); ++i) {
			cooling_dev_t *cdev_config = parser.get_cool_dev_index(i);
			if (!cdev_config)
				continue;

			if (add_replace_cdev(cdev_config) == THD_SUCCESS) {
				current_cdev_index++;
			}
		}
		cdev_cnt = current_cdev_index;
	}

	cthd_cdev_cpufreq *cpu_freq_dev = new cthd_cdev_cpufreq(cdev_cnt, -1);
	cpu_freq_dev->set_cdev_type("cpufreq");
	if (cpu_freq_dev->update() == THD_SUCCESS) {
		cdevs.push_back(cpu_freq_dev);
		++cdev_cnt;
	} else
		delete cpu_freq_dev;

	cthd_sysfs_cdev_rapl_dram *rapl_dram_dev = new cthd_sysfs_cdev_rapl_dram(
			cdev_cnt, 0);
	rapl_dram_dev->set_cdev_type("rapl_controller_dram");
	if (rapl_dram_dev->update() == THD_SUCCESS) {
		cdevs.push_back(rapl_dram_dev);
		++cdev_cnt;
	} else
		delete rapl_dram_dev;

	// Dump all cooling devices
	for (unsigned i = 0; i < cdevs.size(); ++i) {
		cdevs[i]->cdev_dump();
	}

	return THD_SUCCESS;
}

