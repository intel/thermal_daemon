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
#include "thd_sensor_rapl_power.h"
#include "thd_zone_rapl_power.h"

#ifdef GLIB_SUPPORT
#include "thd_cdev_modem.h"
#endif

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
static cooling_dev_t cpu_def_cooling_devices[] = {
		{ true, CDEV_DEF_BIT_UNIT_VAL
				| CDEV_DEF_BIT_READ_BACK | CDEV_DEF_BIT_MIN_STATE | CDEV_DEF_BIT_STEP,
				0, ABSOULUTE_VALUE, 0, 0, 5, false, false, "intel_powerclamp", "", 4,
				false, { 0.0, 0.0, 0.0 },"" },
		{ true, CDEV_DEF_BIT_UNIT_VAL
				| CDEV_DEF_BIT_READ_BACK | CDEV_DEF_BIT_MIN_STATE | CDEV_DEF_BIT_STEP,
				0, ABSOULUTE_VALUE, 0, 100, 5, false, false, "LCD", "", 4, false, { 0.0,
				0.0, 0.0 },"" } };

cthd_engine_default::~cthd_engine_default() {
}

int cthd_engine_default::debug_mode_on(void) {
	static const char *debug_mode = TDRUNDIR
	"/debug_mode";
	struct stat s;

	if (stat(debug_mode, &s))
		return 0;

	return 1;
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
	index = current_sensor_index;

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
		if (index != current_sensor_index)
			break;
	}
	if (index == current_sensor_index) {
		// No coretemp sysfs exist, try hwmon
		thd_log_warn("Thermal DTS: No coretemp sysfs found\n");
	}

	cthd_sensor_kbl_amdgpu_thermal *amdgpu_thermal = new cthd_sensor_kbl_amdgpu_thermal(index);
	if (amdgpu_thermal->sensor_update() == THD_SUCCESS) {
		sensors.push_back(amdgpu_thermal);
		++index;
	} else {
		delete amdgpu_thermal;
	}

	cthd_sensor_kbl_amdgpu_power *amdgpu_power = new cthd_sensor_kbl_amdgpu_power(index);
	if (amdgpu_power->sensor_update() == THD_SUCCESS) {
		sensors.push_back(amdgpu_power);
		++index;
	} else {
		delete amdgpu_power;
	}

	cthd_sensor_kbl_g_mcp *mcp_power = new cthd_sensor_kbl_g_mcp(index);
	if (mcp_power->sensor_update() == THD_SUCCESS) {
		sensors.push_back(mcp_power);
		++index;
	} else {
		delete mcp_power;
	}

	if (debug_mode_on()) {
		// Only used for debug power using ThermalMonitor
		cthd_sensor_rapl_power *rapl_power = new cthd_sensor_rapl_power(index);
		if (rapl_power->sensor_update() == THD_SUCCESS) {
			sensors.push_back(rapl_power);
			++index;
		} else {
			delete rapl_power;
		}
	}

	current_sensor_index = index;
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
				cthd_sensor *sensor_new = NULL;
				if (sensor_config->virtual_sensor) {
					cthd_sensor_virtual *sensor_virt = new cthd_sensor_virtual(
							index, sensor_config->name,
							sensor_config->sensor_link.name,
							sensor_config->sensor_link.multiplier,
							sensor_config->sensor_link.offset);
					if (sensor_virt->sensor_update() != THD_SUCCESS) {
						delete sensor_virt;
						continue;
					}
					sensor_new = sensor_virt;
				} else {
					sensor_new = new cthd_sensor(index, sensor_config->path,
							sensor_config->name,
							SENSOR_TYPE_RAW);
					if (sensor_new->sensor_update() != THD_SUCCESS) {
						delete sensor_new;
						continue;
					}
				}
				if (sensor_new) {
					sensors.push_back(sensor_new);
					++index;
				}
			}
		}
	}

	current_sensor_index = index;

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		sensors[i]->sensor_dump();
	}

	return THD_SUCCESS;
}

bool cthd_engine_default::add_int340x_processor_dev(void)
{
	if (thd_ignore_default_control)
		return false;

	/* Specialized processor thermal device names */
	cthd_zone *processor_thermal = NULL, *acpi_thermal = NULL;
	cthd_INT3400 int3400(uuid);
	unsigned int passive, new_passive = 0, critical = 0;

	if (int3400.match_supported_uuid() == THD_SUCCESS) {
		processor_thermal = search_zone("B0D4");
	}

	if (!processor_thermal)
		processor_thermal = search_zone("B0DB");
	if (!processor_thermal)
		processor_thermal = search_zone("TCPU");

	if (processor_thermal) {
		/* Check If there is a valid passive trip */
		for (unsigned int i = 0; i < processor_thermal->get_trip_count(); ++i) {
			cthd_trip_point *trip = processor_thermal->get_trip_at_index(i);
			if (trip && trip->get_trip_type() == PASSIVE
					&& (passive = trip->get_trip_temp())) {

				/* Need to honor ACPI _CRT, otherwise the system could be shut down by Linux kernel */
				acpi_thermal = search_zone("acpitz");
				if (acpi_thermal) {
					for (unsigned int i = 0; i < acpi_thermal->get_trip_count(); ++i) {
						cthd_trip_point *crit = acpi_thermal->get_trip_at_index(i);
						if (crit && crit->get_trip_type() == CRITICAL) {
							critical = crit->get_trip_temp();
							break;
						}
					}
				}

				if (critical && passive + 5 * 1000 >= critical) {
					new_passive = critical - 15 * 1000;
					if (new_passive < critical)
						trip->thd_trip_update_set_point(new_passive);
				}

				thd_log_info("Processor thermal device is present \n");
				thd_log_info("It will act as CPU thermal zone !! \n");
				thd_log_info("Processor thermal device passive Trip is %d\n",
						trip->get_trip_temp());

				processor_thermal->set_zone_active();

				cthd_cdev *cdev;

				cdev = search_cdev("rapl_controller");
				if (cdev) {
					processor_thermal->bind_cooling_device(PASSIVE, 0, cdev,
							cthd_trip_point::default_influence);
				}
				cdev = search_cdev("intel_pstate");
				if (cdev) {
					processor_thermal->bind_cooling_device(PASSIVE, 0, cdev,
							cthd_trip_point::default_influence);
				}

				cdev = search_cdev("intel_powerclamp");
				if (cdev) {
					processor_thermal->bind_cooling_device(PASSIVE, 0, cdev,
							cthd_trip_point::default_influence);
				}

				cdev = search_cdev("Processor");
				if (cdev) {
					processor_thermal->bind_cooling_device(PASSIVE, 0, cdev,
							cthd_trip_point::default_influence);
				}

				return true;
			}
		}
	}

	return false;
}

void cthd_engine_default::disable_cpu_zone(thermal_zone_t *zone_config) {

	if (parser.thermal_conf_auto()) {
		cthd_zone *cpu_zone = search_zone("cpu");
		if (cpu_zone)
			cpu_zone->set_zone_inactive();
		return;
	}

	cthd_zone *zone = search_zone(zone_config->type);
	if (!zone)
		return;

	if (!zone->zone_active_status())
		return;

	for (unsigned int k = 0; k < zone_config->trip_pts.size(); ++k) {
		trip_point_t &trip_pt_config = zone_config->trip_pts[k];
		cthd_sensor *sensor = search_sensor(trip_pt_config.sensor_type);
		if (sensor && sensor->get_sensor_type() == "B0D4") {
			thd_log_info(
					"B0D4 is defined in thermal-config so deactivating default cpu\n");
			cthd_zone *cpu_zone = search_zone("cpu");
			if (cpu_zone)
				cpu_zone->set_zone_inactive();
		}
	}
}

int cthd_engine_default::read_thermal_zones() {
	int index;
	DIR *dir;
	struct dirent *entry;
	const std::string base_path[] = { "/sys/devices/platform/",
			"/sys/class/hwmon/" };
	int i;

	thd_read_default_thermal_zones();
	index = current_zone_index;

	bool valid_int340x = add_int340x_processor_dev();

	if (!thd_ignore_default_control && !valid_int340x && !search_zone("cpu")) {
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

						cthd_zone_cpu *zone = new cthd_zone_cpu(index,
								base_path[i] + entry->d_name + "/",
								atoi(entry->d_name + strlen("coretemp.")));
						if (zone->zone_update() == THD_SUCCESS) {
							zone->set_zone_active();
							zones.push_back(zone);
							cpu_zone_created = true;
							++index;
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
			thd_log_error(
					"Thermal DTS or hwmon: No Zones present Need to configure manually\n");
		}
	}

	current_zone_index = index;

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

						if (trip_pt_config.dependency.dependency) {
							trip_pt.set_dependency(trip_pt_config.dependency.cdev, trip_pt_config.dependency.state);
						}

						// bind cdev
						for (unsigned int j = 0;
								j < trip_pt_config.cdev_trips.size(); ++j) {
							cthd_cdev *cdev = search_cdev(
									trip_pt_config.cdev_trips[j].type);
							if (cdev) {
								trip_pt.thd_trip_point_add_cdev(*cdev,
										trip_pt_config.cdev_trips[j].influence,
										trip_pt_config.cdev_trips[j].sampling_period,
										trip_pt_config.cdev_trips[j].target_state_valid,
										trip_pt_config.cdev_trips[j].target_state,
										&trip_pt_config.cdev_trips[j].pid_param);
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
										trip_pt_config.cdev_trips[j].influence,
										trip_pt_config.cdev_trips[j].sampling_period,
										trip_pt_config.cdev_trips[j].target_state_valid,
										trip_pt_config.cdev_trips[j].target_state) == THD_SUCCESS) {
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
				cthd_zone_generic *zone = new cthd_zone_generic(index, i,
						zone_config->type);
				if (zone->zone_update() == THD_SUCCESS) {
					zones.push_back(zone);
					++index;
					zone->set_zone_active();
				} else
					delete zone;
			}
			disable_cpu_zone(zone_config);
		}
	}
	current_zone_index = index;

	if (debug_mode_on()) {
		// Only used for debug power using ThermalMonitor
		cthd_zone_rapl_power *rapl_power = new cthd_zone_rapl_power(index);
		if (rapl_power->zone_update() == THD_SUCCESS) {
			rapl_power->set_zone_active();
			zones.push_back(rapl_power);
			++index;
		} else {
			delete rapl_power;
		}
		current_zone_index = index;
	}

	if (!zones.size()) {
		thd_log_info("No Thermal Zones found \n");
		return THD_FATAL_ERROR;
	}
#ifdef AUTO_DETECT_RELATIONSHIP
	def_binding.do_default_binding(cdevs);
#endif
	thd_log_info("\n\n ZONE DUMP BEGIN\n");
	for (unsigned int i = 0; i < zones.size(); ++i) {
		zones[i]->zone_dump();
	}
	thd_log_info("\n\n ZONE DUMP END\n");

	return THD_SUCCESS;
}

int cthd_engine_default::add_replace_cdev(cooling_dev_t *config) {
	cthd_cdev *cdev;
	bool cdev_present = false;
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
		if (config->type_string.compare("intel_modem") == 0) {
#ifdef GLIB_SUPPORT
			/*
			 * Add Modem as cdev
			 * intel_modem is a modem identifier across all intel platforms.
			 * The differences between the modems of various intel platforms
			 * are to be taken care in the cdev implementation.
			 */
			cdev = new cthd_cdev_modem(current_cdev_index, config->path_str);
#endif
		} else
			cdev = new cthd_gen_sysfs_cdev(current_cdev_index, config->path_str);
		if (!cdev)
			return THD_ERROR;
		cdev->set_cdev_type(config->type_string);
		if (cdev->update() != THD_SUCCESS) {
			delete cdev;
			return THD_ERROR;
		}
		cdevs.push_back(cdev);
		++current_cdev_index;
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

	if (config->mask & CDEV_DEF_BIT_WRITE_PREFIX)
		cdev->thd_cdev_set_write_prefix(config->write_prefix);

	return THD_SUCCESS;

}

int cthd_engine_default::read_cooling_devices() {
	int size;
	int i;

	// Read first all the default cooling devices added by kernel
	thd_read_default_cooling_devices();

	// Add RAPL cooling device
	cthd_sysfs_cdev_rapl *rapl_dev = new cthd_sysfs_cdev_rapl(
			current_cdev_index, 0);
	rapl_dev->set_cdev_type("rapl_controller");
	rapl_dev->set_cdev_alias("B0D4");
	if (rapl_dev->update() == THD_SUCCESS) {
		cdevs.push_back(rapl_dev);
		++current_cdev_index;
	} else {
		delete rapl_dev;
		rapl_dev = NULL;
	}

	// Add RAPL mmio cooling device
	if (!disable_active_power && (parser.thermal_matched_platform_index() >= 0 || force_mmio_rapl)) {
		cthd_sysfs_cdev_rapl *rapl_mmio_dev =
				new cthd_sysfs_cdev_rapl(
						current_cdev_index, 0,
						"/sys/devices/virtual/powercap/intel-rapl-mmio/intel-rapl-mmio:0/");
		rapl_mmio_dev->set_cdev_type("rapl_controller_mmio");
		if (rapl_mmio_dev->update() == THD_SUCCESS) {
			cdevs.push_back(rapl_mmio_dev);
			++current_cdev_index;

			// Prefer MMIO access over MSR access for B0D4
			if (rapl_dev) {
				struct adaptive_target target = {};

				rapl_dev->set_cdev_alias("");

				if (adaptive_mode) {
					thd_log_info("Disable rapl-msr interface and use rapl-mmio\n");
					rapl_dev->rapl_update_enable_status(0);

					target.code = "PL1MAX";
					target.argument = "200000";
					rapl_dev->set_adaptive_target(target);
				}
			}
			rapl_mmio_dev->set_cdev_alias("B0D4");
		} else {
			delete rapl_mmio_dev;
		}
	}

	// Add Intel P state driver as cdev
	cthd_intel_p_state_cdev *pstate_dev = new cthd_intel_p_state_cdev(
			current_cdev_index);
	pstate_dev->set_cdev_type("intel_pstate");
	if (pstate_dev->update() == THD_SUCCESS) {
		cdevs.push_back(pstate_dev);
		++current_cdev_index;
	} else
		delete pstate_dev;

	// Add statically defined cooling devices
	size = sizeof(cpu_def_cooling_devices) / sizeof(cooling_dev_t);
	for (i = 0; i < size; ++i) {
		add_replace_cdev(&cpu_def_cooling_devices[i]);
	}

	cthd_cdev_cpufreq *cpu_freq_dev = new cthd_cdev_cpufreq(current_cdev_index,
			-1);
	cpu_freq_dev->set_cdev_type("cpufreq");
	if (cpu_freq_dev->update() == THD_SUCCESS) {
		cdevs.push_back(cpu_freq_dev);
		++current_cdev_index;
	} else
		delete cpu_freq_dev;

	cthd_sysfs_cdev_rapl_dram *rapl_dram_dev = new cthd_sysfs_cdev_rapl_dram(
			current_cdev_index, 0);
	rapl_dram_dev->set_cdev_type("rapl_controller_dram");
	if (rapl_dram_dev->update() == THD_SUCCESS) {
		cdevs.push_back(rapl_dram_dev);
		++current_cdev_index;
	} else
		delete rapl_dram_dev;

	cthd_cdev *cdev = search_cdev("LCD");
	if (!cdev) {
		cthd_cdev_backlight *backlight_dev = new cthd_cdev_backlight(
				current_cdev_index, 0);
		backlight_dev->set_cdev_type("LCD");
		if (backlight_dev->update() == THD_SUCCESS) {
			cdevs.push_back(backlight_dev);
			++current_cdev_index;
		} else
			delete backlight_dev;
	}

	cthd_cdev *cdev_amdgpu = search_cdev("amdgpu");
	if (!cdev_amdgpu) {
		cthd_cdev_kgl_amdgpu *cdev_amdgpu = new cthd_cdev_kgl_amdgpu(
				current_cdev_index, 0);
		cdev_amdgpu->set_cdev_type("amdgpu");
		if (cdev_amdgpu->update() == THD_SUCCESS) {
			cdevs.push_back(cdev_amdgpu);
			++current_cdev_index;
		} else
			delete cdev_amdgpu;
	}

	// Add from XML cooling device config
	if (!parser_init() && parser.platform_matched()) {
		for (int i = 0; i < parser.cdev_count(); ++i) {
			cooling_dev_t *cdev_config = parser.get_cool_dev_index(i);
			if (!cdev_config)
				continue;
			add_replace_cdev(cdev_config);
		}
	}

	// Dump all cooling devices
	for (unsigned i = 0; i < cdevs.size(); ++i) {
		cdevs[i]->cdev_dump();
	}

	return THD_SUCCESS;
}

// Thermal engine
cthd_engine *thd_engine;

int thd_engine_create_default_engine(bool ignore_cpuid_check,
		bool exclusive_control, const char *conf_file) {
	int res;
	thd_engine = new cthd_engine_default();
	if (!thd_engine)
		return THD_ERROR;

	if (exclusive_control)
		thd_engine->set_control_mode(EXCLUSIVE);

	// Initialize thermald objects
	thd_engine->set_poll_interval(thd_poll_interval);
	if (conf_file)
		thd_engine->set_config_file(conf_file);

	res = thd_engine->thd_engine_init(ignore_cpuid_check);
	if (res != THD_SUCCESS) {
		if (res == THD_FATAL_ERROR)
			thd_log_error("THD engine init failed\n");
		else
			thd_log_msg("THD engine init failed\n");
	}

	res = thd_engine->thd_engine_start();
	if (res != THD_SUCCESS) {
		if (res == THD_FATAL_ERROR)
			thd_log_error("THD engine start failed\n");
		else
			thd_log_msg("THD engine start failed\n");
	}

	return res;
}

void cthd_engine_default::workarounds()
{
	// Every 30 seconds repeat
	if (!disable_active_power && !workaround_interval) {
		workaround_rapl_mmio_power();
		workaround_tcc_offset();
		workaround_interval = 7;
	} else {
		--workaround_interval;
	}
}

#ifndef ANDROID
#include <cpuid.h>
#include <sys/mman.h>
#define BIT_ULL(nr)	(1ULL << (nr))
#endif

void cthd_engine_default::workaround_rapl_mmio_power(void)
{
	if (!workaround_enabled)
		return;

	cthd_cdev *cdev = search_cdev("rapl_controller_mmio");
	if (cdev) {
		/* RAPL MMIO is enabled and getting used. No need to disable */
		return;
	} else {
		csys_fs _sysfs("/sys/devices/virtual/powercap/intel-rapl-mmio/intel-rapl-mmio:0/");

		if (_sysfs.exists()) {
			std::stringstream temp_str;

			temp_str << "enabled";
			if (_sysfs.write(temp_str.str(), 0) > 0)
				return;

			thd_log_debug("Failed to write to RAPL MMIO\n");
		}
	}

#ifndef ANDROID
	int map_fd;
	void *rapl_mem;
	unsigned char *rapl_pkg_pwr_addr;
	unsigned long long pkg_power_limit;

	unsigned int ebx, ecx, edx;
	unsigned int fms, family, model;

	csys_fs sys_fs;

	ecx = edx = 0;
	__cpuid(1, fms, ebx, ecx, edx);
	family = (fms >> 8) & 0xf;
	model = (fms >> 4) & 0xf;
	if (family == 6 || family == 0xf)
		model += ((fms >> 16) & 0xf) << 4;

	// Apply for KabyLake only
	if (model != 0x8e && model != 0x9e)
		return;

	map_fd = open("/dev/mem", O_RDWR, 0);
	if (map_fd < 0)
		return;

	rapl_mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd,
			0xfed15000);
	if (!rapl_mem || rapl_mem == MAP_FAILED) {
		close(map_fd);
		return;
	}

	rapl_pkg_pwr_addr = ((unsigned char *)rapl_mem + 0x9a0);
	pkg_power_limit = *(unsigned long long *)rapl_pkg_pwr_addr;
	*(unsigned long long *)rapl_pkg_pwr_addr = pkg_power_limit
			& ~BIT_ULL(15);

	munmap(rapl_mem, 4096);
	close(map_fd);
#endif
}

void cthd_engine_default::workaround_tcc_offset(void)
{
#ifndef ANDROID
	csys_fs sys_fs;
	int tcc;

	if (tcc_offset_checked && tcc_offset_low)
		return;

	if (parser.thermal_matched_platform_index() < 0) {
		tcc_offset_checked = 1;
		tcc_offset_low = 1;
		return;
	}

	if (sys_fs.exists("/sys/bus/pci/devices/0000:00:04.0/tcc_offset_degree_celsius")) {
		if (sys_fs.read("/sys/bus/pci/devices/0000:00:04.0/tcc_offset_degree_celsius", &tcc) <= 0) {
			tcc_offset_checked = 1;
			tcc_offset_low = 1;
			return;
		}

		if (tcc > 10) {
			int ret;

			ret = sys_fs.write("/sys/bus/pci/devices/0000:00:04.0/tcc_offset_degree_celsius", 5);
			if (ret < 0)
				tcc_offset_low = 1; // probably locked so retryA

			tcc_offset_checked = 1;
		} else {
			if (!tcc_offset_checked)
				tcc_offset_low = 1;
			tcc_offset_checked = 1;
		}
	} else {
		csys_fs msr_sysfs;
		int ret;

		if(msr_sysfs.exists("/dev/cpu/0/msr")) {
			unsigned long long val = 0;

			ret = msr_sysfs.read("/dev/cpu/0/msr", 0x1a2, (char *)&val, sizeof(val));
			if (ret > 0) {
				int tcc;

				tcc = (val >> 24) & 0xff;
				if (tcc > 10) {
					val &= ~(0xff << 24);
					val |= (0x05 << 24);
					msr_sysfs.write("/dev/cpu/0/msr", 0x1a2, val);
					tcc_offset_checked = 1;
				} else {
					if (!tcc_offset_checked)
						tcc_offset_low = 1;
					tcc_offset_checked = 1;
				}
			}
		}
	}
#endif
}
