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
#include <linux/input.h>
#include <sys/types.h>
#include "thd_engine_adaptive.h"

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
			psv_zone = "TCPU";
			zone = search_zone(psv_zone);
		}

		if (!zone) {
			if (!psv_zone.compare(0, 4, "TCPU")) {
				psv_zone = "B0D4";
				zone = search_zone(psv_zone);
			}
			if (!zone) {
				thd_log_warn("Unable to find a zone for %s\n",
						psv_zone.c_str());
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
			psv_cdev = "B0D4";
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

	int temp = DECI_KELVIN_TO_CELSIUS(psv->temp) * 1000;
	int target_state = 0;

	if (psv->limit.length()) {
		if (!strncasecmp(psv->limit.c_str(), "MAX", 3)) {
			target_state = TRIP_PT_INVALID_TARGET_STATE;
		} else if (!strncasecmp(psv->limit.c_str(), "MIN", 3)) {
			target_state = 0;
		} else {
			std::istringstream buffer(psv->limit);
			buffer >> target_state;
			target_state *= 1000;
		}
	}

	cthd_trip_point trip_pt(zone->get_trip_count(), PASSIVE, temp, 0,
			zone->get_zone_index(), sensor->get_index(), SEQUENTIAL);
	trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence,
			psv->sample_period / 10, target_state ? 1 : 0, target_state,
			NULL, 0, 0, 0);
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

void cthd_engine_adaptive::psvt_consolidate() {
	/* Once all tables are installed, we need to consolidate since
	 * thermald has different implementation.
	 * If there is only entry of type MAX, then simply use thermald default at temperature + 1
	 * If there is a next trip after MAX for a target, then choose a temperature limit in the middle
	 */
	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *zone = zones[i];
		unsigned int count = zone->get_trip_count();

		// Special case for handling a single trip which has a defined
		// target state. Add another trip + 1C so that control is applied
		// till max state
		// Count is 2, because there is a default poll trip
		if (count == 2) {
			cthd_trip_point *trip = zone->get_trip_at_index(0);
			int target_state;

			thd_log_info("Single trip with a target state\n");

			if (trip->is_target_valid(target_state) == THD_SUCCESS) {

				cthd_trip_point trip_pt(1, PASSIVE, trip->get_trip_temp() + 1000, 0,
						zone->get_zone_index(), trip->get_sensor_id(), SEQUENTIAL);

				trip_pt.thd_trip_point_add_cdev(*trip->get_first_cdev(), cthd_trip_point::default_influence);

				zone->add_trip(trip_pt, 1);
				continue;
			}
		}

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

#define DEFAULT_SAMPLE_TIME_SEC		5

int cthd_engine_adaptive::install_itmt(struct itmt_entry *itmt_entry) {
	std::string itmt_zone;

	size_t pos = itmt_entry->target.find_last_of(".");
	if (pos == std::string::npos)
		itmt_zone = itmt_entry->target;
	else
		itmt_zone = itmt_entry->target.substr(pos + 1);

	while (itmt_zone.back() == '_') {
		itmt_zone.pop_back();
	}

	cthd_zone *zone = search_zone(itmt_zone);
	if (!zone) {
		if (!itmt_zone.compare(0, 4, "B0D4")) {
			itmt_zone = "TCPU";
			zone = search_zone(itmt_zone);
		}

		if (!zone) {
			if (!itmt_zone.compare(0, 4, "TCPU")) {
				itmt_zone = "B0D4";
				zone = search_zone(itmt_zone);
			}
			if (!zone) {
				thd_log_warn("Unable to find a zone for %s\n",
						itmt_zone.c_str());
				return THD_ERROR;
			}
		}
	}

	cthd_cdev *cdev = search_cdev("rapl_controller_mmio");

	if (!cdev) {
		return THD_ERROR;
	}

	cthd_sensor *sensor = search_sensor(itmt_zone);
	if (!sensor) {
		thd_log_warn("Unable to find a sensor for %s\n", itmt_zone.c_str());
		return THD_ERROR;
	}

	int temp = (itmt_entry->trip_point - 2732) * 100;
	int _min_state = 0, _max_state = 0;

	if (itmt_entry->pl1_max.length()) {
		if (!strncasecmp(itmt_entry->pl1_max.c_str(), "MAX", 3)) {
			_max_state = TRIP_PT_INVALID_TARGET_STATE;
		} else if (!strncasecmp(itmt_entry->pl1_max.c_str(), "MIN", 3)) {
			_max_state = 0;
		} else {
			std::istringstream buffer(itmt_entry->pl1_max);
			buffer >> _max_state;
			_max_state *= 1000;
		}
	}

	if (itmt_entry->pl1_min.length()) {
		if (!strncasecmp(itmt_entry->pl1_min.c_str(), "MAX", 3)) {
			_min_state = TRIP_PT_INVALID_TARGET_STATE;
		} else if (!strncasecmp(itmt_entry->pl1_min.c_str(), "MIN", 3)) {
			_min_state = 0;
		} else {
			std::istringstream buffer(itmt_entry->pl1_min);
			buffer >> _min_state;
			_min_state *= 1000;
		}
	}

	cthd_trip_point trip_pt(zone->get_trip_count(), PASSIVE, temp, 0,
			zone->get_zone_index(), sensor->get_index(), SEQUENTIAL);

	/*
	 * Why the min = max and max=min in the below
	 * thd_trip_point_add_cdev?
	 *
	 * Thermald min_state is where no cooling is active
	 * Thermald max_state is where max cooling is applied
	 *
	 * If you check one ITMT table entry:
	 *  target:\_SB_.PC00.LPCB.ECDV.CHRG  trip_temp:45 pl1_min:28000 pl1.max:MAX
	 *
	 * This means that when exceeding 45 set the PL1 to 28W,
	 * Below 45 PL1 is set to maximum (full power)
	 *
	 * That means that untrottled case is PL1_MAX
	 * Throttled case is PL1_MIN
	 * Which is opposite of the argument order in the
	 * thd_trip_point_add_cdev()
	 */
	trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence,
	DEFAULT_SAMPLE_TIME_SEC, 0, 0,
	NULL, 1, _max_state, _min_state);

	zone->add_trip(trip_pt, 1);
	zone->zone_cdev_set_binded();
	zone->set_zone_active();

	return 0;
}

int cthd_engine_adaptive::set_itmt_target(struct adaptive_target &target) {
	struct itmt *itmt;

	thd_log_info("set_int3400 ITMT target %s\n", target.argument.c_str());

	itmt = gddv.find_itmt(target.argument);
	if (!itmt) {
		return THD_ERROR;
	}

	if (!int3400_installed) {
		for (unsigned int i = 0; i < zones.size(); ++i) {
			cthd_zone *_zone = zones[i];

			// This is only for debug to plot power, so keep
			if (_zone->get_zone_type() == "rapl_pkg_power")
				continue;

			_zone->zone_reset(1);
			_zone->trip_delete_all();

			if (_zone->zone_active_status())
				_zone->set_zone_inactive();
		}
	}

	for (int i = 0; i < (int) itmt->itmt_entries.size(); i++) {
		install_itmt(&itmt->itmt_entries[i]);
	}

	return THD_SUCCESS;

}

void cthd_engine_adaptive::set_int3400_target(struct adaptive_target &target) {

	if (target.code == "ITMT") {
		if (set_itmt_target(target) == THD_SUCCESS) {
			int3400_installed = 1;
		}
	}

	if (target.code == "PSVT") {
		struct psvt *psvt;

		thd_log_info("set_int3400 target %s\n", target.argument.c_str());

		psvt = gddv.find_psvt(target.argument);
		if (!psvt) {
			return;
		}

		if (!int3400_installed) {
			for (unsigned int i = 0; i < zones.size(); ++i) {
				cthd_zone *_zone = zones[i];

				// This is only for debug to plot power, so keep
				if (_zone->get_zone_type() == "rapl_pkg_power")
					continue;

				_zone->zone_reset(1);
				_zone->trip_delete_all();

				if (_zone->zone_active_status())
					_zone->set_zone_inactive();
			}
		}

		for (int i = 0; i < (int) psvt->psvs.size(); i++) {
			install_passive(&psvt->psvs[i]);
		}
		int3400_installed = 1;
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

	if (target.code == "PSV") {
		set_trip(target.participant, target.argument);
	}
}

void cthd_engine_adaptive::install_passive_default() {
	thd_log_info("IETM_D0 processed\n");

	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *_zone = zones[i];
		_zone->zone_reset(1);
		_zone->trip_delete_all();

		if (_zone->zone_active_status())
			_zone->set_zone_inactive();
	}

	struct psvt *psvt = gddv.find_def_psvt();
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

void cthd_engine_adaptive::execute_target(struct adaptive_target &target) {
	cthd_cdev *cdev;
	std::string name;
	int argument;

	thd_log_info("Target Name:%s\n", target.name.c_str());

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
	} catch (...) {
		thd_log_info("Invalid target target:%s %s\n", target.code.c_str(),
				target.argument.c_str());
		return;
	}
	thd_log_info("target:%s %d\n", target.code.c_str(), argument);
	if (cdev)
		cdev->set_adaptive_target(target);
}

void cthd_engine_adaptive::exec_fallback_target(int target) {
	thd_log_debug("exec_fallback_target %d\n", target);
	int3400_installed = 0;
	for (int i = 0; i < (int) gddv.targets.size(); i++) {
		if (gddv.targets[i].target_id != (uint64_t) target)
			continue;
		execute_target(gddv.targets[i]);
	}
}

// Called every polling interval
void cthd_engine_adaptive::update_engine_state() {

	int target = -1;

	// When This means that gddv doesn't have any conditions
	// no need to do any processing
	if (passive_def_only)
		return;

	if (fallback_id < 0)
		target = gddv.evaluate_conditions();

	if (current_matched_target == target) {
		thd_log_debug("No change in target\n");
		return;
	}

	current_matched_target = target;

	// No target matched
	// It is possible that the target which matched last time didn't match
	// because of conditions have changed.
	// So in that case we have to install the default target.
	// Return only when there is a fallback ID was identified during
	// start after installing that target
	if (target == -1) {
		if (fallback_id >= 0 && !policy_active) {
			exec_fallback_target(gddv.targets[fallback_id].target_id);
			policy_active = 1;
			return;
		}
	}

	int3400_installed = 0;

	if (target > 0) {
		for (int i = 0; i < (int) gddv.targets.size(); i++) {
			if (gddv.targets[i].target_id != (uint64_t) target)
				continue;
			execute_target(gddv.targets[i]);
		}
		policy_active = 1;
	}

	if (!int3400_installed) {
		thd_log_info("Adaptive target doesn't have PSVT or ITMT target\n");
		install_passive_default();
	}
}

int cthd_engine_adaptive::thd_engine_init(bool ignore_cpuid_check,
		bool adaptive) {
	csys_fs sysfs("");
	char *buf;
	size_t size;
	int res;

	parser_disabled = true;
	force_mmio_rapl = true;

	if (!ignore_cpuid_check) {
		check_cpu_id();
		if (!processor_id_match()) {
			thd_log_msg("Unsupported cpu model or platform\n");
			thd_log_msg("Try option --ignore-cpuid-check to disable this compatibility test\n");
			exit(EXIT_SUCCESS);
		}
	}

	csys_fs _sysfs("/tmp/ignore_adaptive");
	if (_sysfs.exists()) {
		return THD_ERROR;
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

	if (sysfs.read(int3400_base_path + "firmware_node/path", int3400_path)
			< 0) {
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

	if (sysfs.read(int3400_base_path + "data_vault", buf, size) < int(size)) {
		thd_log_debug("Unable to read GDDV data vault\n");
		delete[] buf;
		return THD_FATAL_ERROR;
	}

	delete[] buf;

	res = gddv.gddv_init();
	if (res != THD_SUCCESS) {
		return res;
	}

	if (!gddv.conditions.size()) {
		thd_log_info("No adaptive conditions present\n");

		struct psvt *psvt = gddv.find_def_psvt();

		if (psvt) {
			thd_log_info("IETM.D0 found\n");
			passive_def_only = 1;
		} else {
			return THD_SUCCESS;
		}
	}

	/* Read the sensors/zones */
	res = cthd_engine::thd_engine_init(ignore_cpuid_check, adaptive);
	if (res != THD_SUCCESS)
		return res;

	return THD_SUCCESS;
}

int cthd_engine_adaptive::thd_engine_start() {
	if (passive_def_only) {
		// This means there are no conditions present
		// This doesn't mean that there are conditions present but none matched.
		install_passive_default();
		return cthd_engine::thd_engine_start();
	}

	if (gddv.verify_conditions()) {
		// This means that gddv has conditions which thermald can't support
		// Doesn't mean that the conditions supported by thermald matches
		// but can't satisfy any conditions as they don't match.
		// That can be only found during execution of conditions.
		thd_log_info(
				"Some conditions are not supported, so check if any condition set can be matched\n");
		int target = gddv.evaluate_conditions();
		if (target == -1) {
			thd_log_info("Also unable to evaluate any conditions\n");
			thd_log_info(
					"Falling back to use configuration with the highest power\n");

			int i = gddv.find_agressive_target();
			thd_log_info("target:%d\n", i);
			if (i >= 0) {
				thd_log_info("fallback id:%d\n", i);
				fallback_id = i;
			} else {
				struct psvt *psvt = gddv.find_def_psvt();

				if (psvt) {
					thd_log_info("IETM.D0 found\n");
					install_passive_default();
				} else {
					gddv.gddv_free();
					return THD_ERROR;
				}
			}
		}
	}

	set_control_mode(EXCLUSIVE);

	// Check if any conditions can be satisfied at this time
	// If not just install the default passive IETM.D0 table
	if (gddv.evaluate_conditions() == -1)
		install_passive_default();

	thd_log_info("adaptive engine reached end\n");

	return cthd_engine::thd_engine_start();
}

int thd_engine_create_adaptive_engine(bool ignore_cpuid_check, bool test_mode) {
	thd_engine = new cthd_engine_adaptive();

	thd_engine->set_poll_interval(thd_poll_interval);

	// Initialize thermald objects
	if (thd_engine->thd_engine_init(ignore_cpuid_check, true) != THD_SUCCESS) {
		thd_log_info("THD engine init failed\n");
		return THD_ERROR;
	}

	if (thd_engine->thd_engine_start() != THD_SUCCESS) {
		thd_log_info("THD engine start failed\n");
		if (test_mode) {
			thd_log_warn("This platform doesn't support adaptive mode\n");
			thd_log_warn(
					"It is possible that manufacturer doesn't support DPTF tables or\n");
			thd_log_warn(
					"didn't provide tables, which can be parsed in open source.\n");
			exit(0);
		}
		return THD_ERROR;
	}

	return THD_SUCCESS;
}
