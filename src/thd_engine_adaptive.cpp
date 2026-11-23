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
#include <fcntl.h>
#include <inttypes.h>
#include <linux/input.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include "thd_engine_adaptive.h"
#include "thd_sensor_rapl_power.h"
#include "thd_zone_dynamic.h"

int cthd_engine_adaptive::install_passive(struct psv *psv) {
	std::string psv_zone;

	size_t pos = psv->target.find_last_of('.');
	if (pos == std::string::npos)
		psv_zone = psv->target;
	else
		psv_zone = psv->target.substr(pos + 1);

	while (!psv_zone.empty() && psv_zone.back() == '_') {
		psv_zone.pop_back();
	}

	psv_zone = cpu_participant_name(psv_zone, false);
	cthd_zone *zone = search_zone(psv_zone);
	if (!zone) {
		thd_log_warn("Unable to find a zone for %s\n", psv_zone.c_str());
		return THD_ERROR;
	}

	std::string psv_cdev;
	pos = psv->source.find_last_of('.');
	if (pos == std::string::npos)
		psv_cdev = psv->source;
	else
		psv_cdev = psv->source.substr(pos + 1);

	while (!psv_cdev.empty() && psv_cdev.back() == '_') {
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

	/* millicelsius: an absurd PSVT temperature would overflow int */
	long long temp_mc = (long long) DECI_KELVIN_TO_CELSIUS(psv->temp) * 1000;
	if (temp_mc < INT_MIN || temp_mc > INT_MAX) {
		thd_log_warn("PSVT %s -> %s: temperature %d dK out of range, ignored\n",
				psv->source.c_str(), psv->target.c_str(), psv->temp);
		return THD_ERROR;
	}
	int temp = (int) temp_mc;
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

	/*
	 * CPU power limit entries with a step size and coefficients: step
	 * control along the curve of all such entries of this sensor, see
	 * cthd_trip_point::step_control(). The sampling period is in 0.1 s.
	 */
	if (psv->step_size > 0 && psv->limit_coeff > 0 && psv->unlimit_coeff > 0
			&& psv->step_size <= INT_MAX / 1000
			&& (!psv_cdev.compare(0, 4, "TCPU")
					|| !psv_cdev.compare(0, 4, "B0D4"))) {
		step_point_t point;

		point.temp = temp;
		if (target_state == TRIP_PT_INVALID_TARGET_STATE)
			point.limit = STEP_LIMIT_NONE;
		else if (!target_state)
			point.limit = STEP_LIMIT_FULL;
		else
			point.limit = target_state;
		point.step = psv->step_size * 1000;
		point.limit_coeff = psv->limit_coeff;
		point.unlimit_coeff = psv->unlimit_coeff;
		point.max_entry = 0;

		for (unsigned int i = 0; i < zone->get_trip_count(); ++i) {
			cthd_trip_point *trip = zone->get_trip_at_index(i);

			if (trip && trip->is_step_control()
					&& trip->get_sensor_id() == sensor->get_index()
					&& trip->get_first_cdev() == cdev) {
				trip->thd_trip_point_add_step_point(*cdev, point,
						psv->sample_period);
				zone->zone_cdev_set_binded();
				zone->set_zone_active();
				return 0;
			}
		}

		cthd_trip_point trip_pt(zone->get_trip_count(), PASSIVE, temp, 0,
				zone->get_zone_index(), sensor->get_index(), SEQUENTIAL);
		trip_pt.thd_trip_point_add_step_point(*cdev, point,
				psv->sample_period);
		zone->add_trip(trip_pt, 1);
		zone->zone_cdev_set_binded();
		zone->set_zone_active();
		return 0;
	}

	cthd_trip_point trip_pt(zone->get_trip_count(), PASSIVE, temp, 0,
			zone->get_zone_index(), sensor->get_index(), SEQUENTIAL);
	trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence,
			psv->sample_period / 10, target_state ? 1 : 0, target_state,
			nullptr, 0, 0, 0);
	zone->add_trip(trip_pt, 1);
	zone->zone_cdev_set_binded();
	zone->set_zone_active();

	return 0;
}

void cthd_engine_adaptive::set_trip(const std::string& target, const std::string& argument) {
	std::string psv_zone;
	float float_temp = stof(argument, nullptr);
	int temp = (int) (float_temp * 1000);

	size_t pos = target.find_last_of('.');

	if (pos == std::string::npos)
		psv_zone = target;
	else
		psv_zone = target.substr(pos + 1);

	while (!psv_zone.empty() && psv_zone.back() == '_') {
		psv_zone.pop_back();
	}

	psv_zone = cpu_participant_name(psv_zone, false);
	cthd_zone *zone = search_zone(psv_zone);
	if (!zone) {
		thd_log_warn("Unable to find a zone for %s\n", psv_zone.c_str());
		return;
	}

	int index = 0;
	cthd_trip_point *trip = zone->get_trip_at_index(index);
	while (trip != nullptr) {
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
		cthd_zone *zone = zones[i].get();
		unsigned int count = zone->get_trip_count();

		// Step control trips carry their own curve
		for (unsigned int j = 0; j < count; ++j) {
			cthd_trip_point *trip = zone->get_trip_at_index(j);

			if (trip && trip->is_step_control())
				trip->step_points_finalize();
		}

		// Special case for handling a single trip which has a defined
		// target state. Add another trip + 1C so that control is applied
		// till max state
		// Count is 2, because there is a default poll trip
		if (count == 2 && !zone->get_trip_at_index(0)->is_step_control()) {
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
			if (trip->is_step_control())
				continue;
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

	size_t pos = itmt_entry->target.find_last_of('.');
	if (pos == std::string::npos)
		itmt_zone = itmt_entry->target;
	else
		itmt_zone = itmt_entry->target.substr(pos + 1);

	while (!itmt_zone.empty() && itmt_zone.back() == '_') {
		itmt_zone.pop_back();
	}

	itmt_zone = cpu_participant_name(itmt_zone, false);
	cthd_zone *zone = search_zone(itmt_zone);
	if (!zone) {
		thd_log_warn("Unable to find a zone for %s\n", itmt_zone.c_str());
		return THD_ERROR;
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

	cthd_trip_point trip_pt(zone->get_trip_count(), PASSIVE, temp, itmt_hyst,
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
	pid_param_t pid_param;

	if (itmt_pid_config.enable) {
		/* PL1 range of the entry: MAX/MIN resolved to the device range */
		long long top = (_max_state && _max_state != TRIP_PT_INVALID_TARGET_STATE) ?
				_max_state : cdev->get_min_state();
		long long bottom = (_min_state && _min_state != TRIP_PT_INVALID_TARGET_STATE) ?
				_min_state : cdev->get_max_state();
		long long range = top > bottom ? top - bottom : bottom - top;
		double scale = itmt_pid_config.scale(range);

		pid_param.valid = 1;
		pid_param.kp = itmt_pid_config.kp * scale;
		pid_param.ki = 0;
		pid_param.kd = itmt_pid_config.kd * scale;
		pid_param.mode = PID_INCREMENTAL;
		pid_param.gradual_release = 1;
		pid_param.set_point_offset = itmt_pid_config.set_point_offset;
		pid_param.time_scaled = 1;
		thd_log_info("ITMT PID %s: PL1 range %lld mW, gain scale %.2f\n",
				itmt_zone.c_str(), range / 1000, scale);
	}

	trip_pt.thd_trip_point_add_cdev(*cdev, cthd_trip_point::default_influence,
	DEFAULT_SAMPLE_TIME_SEC, 0, 0,
	itmt_pid_config.enable ? &pid_param : nullptr, 1, _max_state, _min_state);

	zone->add_trip(trip_pt, 1);
	zone->zone_cdev_set_binded();
	zone->set_zone_active();

	return 0;
}

/*
 * A change of the adaptive target installs the new target's tables: the
 * zones are reset first, which restores the power limit, also while it is
 * being limited. Keep a power limit that is in force over the change instead:
 * a temporary trip holds it as long as the sensor of the trip that limited
 * it stays at or above that trip (an ITMT trip: minus its hysteresis), then
 * releases it at that trip's rate (a PSVT entry: step size * unlimit
 * coefficient per its sampling period; ITMT: the device's release step per
 * second). The new target's trips apply as usual; the most restrictive
 * request wins.
 */
void cthd_engine_adaptive::carry_capture() {
	carries.clear();

	for (unsigned int i = 0; i < cdevs.size(); ++i) {
		cthd_cdev *cdev = cdevs[i].get();
		int zone_id, trip_id;

		/* Power limits (inverted range) that are limited now */
		if (cdev->get_min_state() <= cdev->get_max_state() || cdev->in_min_state())
			continue;
		if (!cdev->active_limit_source(&zone_id, &trip_id))
			continue;

		cthd_zone *zone = nullptr;
		for (unsigned int j = 0; j < zones.size(); ++j) {
			if (zones[j]->get_zone_index() == zone_id)
				zone = zones[j].get();
		}
		if (!zone)
			continue;

		cthd_trip_point *trip = nullptr;
		for (unsigned int j = 0; j < zone->get_trip_count(); ++j) {
			cthd_trip_point *t = zone->get_trip_at_index(j);
			if (t && t->get_trip_index() == trip_id)
				trip = t;
		}
		if (!trip)
			continue;

		power_limit_carry_t c;
		c.cdev = cdev;
		c.zone_index = zone_id;
		c.sensor_id = trip->get_sensor_id();
		c.request = cdev->get_curr_state();

		const trip_pt_cdev_t *sc = trip->step_cdev();
		if (sc) {
			const step_point_t &p = sc->step_points[sc->step_band > 0 ? sc->step_band : 0];
			c.hold_temp = sc->step_points[0].temp;
			c.step = p.step;
			c.unlimit_coeff = p.unlimit_coeff;
			c.sampling_period = sc->step_period_ms / 100;
		} else {
			c.hold_temp = (int) trip->get_trip_temp() - (int) trip->get_trip_hyst();
			c.step = cdev->get_release_step();
			if (c.step <= 0)
				c.step = 500000;
			c.unlimit_coeff = 1;
			c.sampling_period = 10;
		}

		thd_log_info("target change: keep %s at %d while zone %d sensor %d >= %d\n",
				cdev->get_cdev_type().c_str(), c.request, zone_id, c.sensor_id,
				c.hold_temp);
		carries.push_back(c);
	}
}

void cthd_engine_adaptive::carry_apply() {
	for (const power_limit_carry_t &c : carries) {
		cthd_zone *zone = nullptr;

		for (unsigned int j = 0; j < zones.size(); ++j) {
			if (zones[j]->get_zone_index() == c.zone_index)
				zone = zones[j].get();
		}
		if (!zone)
			continue;

		int index = carry_trip_index++;
		cthd_trip_point trip(index, PASSIVE, c.hold_temp, 0, c.zone_index,
				c.sensor_id, SEQUENTIAL);

		trip.carry_init(*c.cdev, c.hold_temp, c.request, c.step, c.unlimit_coeff,
				c.sampling_period);
		zone->add_trip(trip, 1);
		zone->zone_cdev_set_binded();
		zone->set_zone_active();
		c.cdev->thd_cdev_set_step_request(c.zone_index, index, c.request, true);
	}
	carries.clear();
}

int cthd_engine_adaptive::set_itmt_target(struct adaptive_target &target) {
	struct itmt *itmt;

	thd_log_info("set_int3400 ITMT target %s\n", target.argument.c_str());

	itmt = gddv.find_itmt(target.argument);
	if (!itmt) {
		return THD_ERROR;
	}

	if (!int3400_installed) {
		carry_capture();
		for (unsigned int i = 0; i < zones.size(); ++i) {
			cthd_zone *_zone = zones[i].get();

			// This is only for debug to plot power, so keep
			if (debug_mode_on()) {
				if (_zone->get_zone_type() == "rapl_pkg_power" || _zone->get_zone_type() == "power_floor")
					continue;
			}

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

// Install the passive trips of a PSVT. Unless another PSVT or ITMT is
// already installed, first clear the trips of the other zones. Return
// the number of trips installed.
int cthd_engine_adaptive::install_psvt(struct psvt *psvt) {
	int installed = 0;

	if (!int3400_installed) {
		carry_capture();
		for (unsigned int i = 0; i < zones.size(); ++i) {
			cthd_zone *_zone = zones[i].get();

			// This is only for debug to plot power, so keep
			if (_zone->get_zone_type() == "rapl_pkg_power" || _zone->get_zone_type() == "power_floor")
				continue;

			_zone->zone_reset(1);
			_zone->trip_delete_all();

			if (_zone->zone_active_status())
				_zone->set_zone_inactive();
		}
	}

	for (unsigned int i = 0; i < psvt->psvs.size(); ++i) {
		if (install_passive(&psvt->psvs[i]) == THD_SUCCESS)
			++installed;
	}

	return installed;
}

// Number of thermal zones, cooling devices and powercap zones
static int thermal_device_count() {
	int count = 0;

	for (const char *path : { "/sys/class/thermal", "/sys/class/powercap" }) {
		DIR *dir = opendir(path);
		if (!dir)
			continue;

		struct dirent *entry;
		while ((entry = readdir(dir)) != nullptr) {
			if (entry->d_name[0] != '.')
				count++;
		}
		closedir(dir);
	}

	return count;
}

void cthd_engine_adaptive::set_int3400_target(struct adaptive_target &target) {

	/*
	 * Only these IETM target codes are handled. Ignore any other (e.g.
	 * EPOT): running the PSVT consolidation, zone dump and zone checks
	 * below again for them changed the trips installed for this target.
	 */
	if (target.code != "ITMT" && target.code != "ITMT3" &&
			target.code != "PSVT" && target.code != "PSV") {
		thd_log_info("Unsupported IETM target code %s (target %s), ignored\n",
				target.code.c_str(), target.name.c_str());
		return;
	}

	if (target.code == "ITMT" || target.code == "ITMT3") {
		if (set_itmt_target(target) == THD_SUCCESS) {
			int3400_installed = 1;
		}
	}

	if (target.code == "PSVT") {
		struct psvt *psvt;
		int installed = 0;

		thd_log_info("set_int3400 target %s\n", target.argument.c_str());

		psvt = gddv.find_psvt(target.argument);
		if (psvt) {
			installed = install_psvt(psvt);
		} else {
			/*
			 * Some firmware references PSVT names that are not in the
			 * GDDV. Installing nothing makes adaptive mode exit for lack
			 * of zones. Use the default PSVT, the platform's baseline
			 * policy, instead.
			 */
			psvt = gddv.find_def_psvt();
			if (psvt) {
				thd_log_warn("PSVT %s not found in GDDV, using default PSVT %s\n",
						target.argument.c_str(), psvt->name.c_str());
				installed = install_psvt(psvt);
			} else {
				thd_log_warn("PSVT %s not found in GDDV, and no default PSVT\n",
						target.argument.c_str());
			}
		}

		if (installed)
			int3400_installed = 1;
	}

	if (int3400_installed)
		psvt_consolidate();
	carry_apply();

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

		// At boot, drivers can probe after thermald started. If thermal
		// devices appeared since, try adaptive mode once more.
		const char *flag = "ignore_adaptive";
		csys_fs retried(TDRUNDIR "/adaptive_retried");
		if (thermal_device_count() > thermal_devices_at_init
				&& !retried.exists()) {
			thd_log_warn("Thermal devices appeared since startup, retry adaptive mode via systemd\n");
			flag = "adaptive_retried";
		} else {
			thd_log_warn("Restart in non adaptive mode via systemd\n");
		}

		char *resolved_rundir = realpath(TDRUNDIR, nullptr);
		if (!resolved_rundir) {
			thd_log_warn("Failed to resolve runtime dir %s: %s\n", TDRUNDIR,
					strerror(errno));
		} else {
			int dir_fd = open(resolved_rundir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (dir_fd < 0) {
				thd_log_warn("Failed to open runtime dir %s: %s\n", resolved_rundir,
						strerror(errno));
			} else {
				struct stat st;
				if (fstat(dir_fd, &st) < 0) {
					thd_log_warn("Failed to stat runtime dir %s: %s\n", resolved_rundir,
							strerror(errno));
				} else if (!S_ISDIR(st.st_mode)) {
					thd_log_warn("Runtime path %s is not a directory\n", resolved_rundir);
				} else if (st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH))) {
					thd_log_warn("Insecure runtime dir permissions on %s (uid=%u mode=%o)\n",
							resolved_rundir, st.st_uid, st.st_mode & 0777);
				} else {
					int fd = openat(dir_fd, flag,
							O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
					if (fd < 0) {
						thd_log_warn("Failed to create %s/%s securely: %s\n",
								resolved_rundir, flag, strerror(errno));
					} else {
						close(fd);
					}
				}
				close(dir_fd);
			}
			free(resolved_rundir);
		}
		exit(EXIT_FAILURE);
	}

	if (target.code == "PSV") {
		set_trip(target.participant, target.argument);
	}
}

void cthd_engine_adaptive::install_passive_default() {
	thd_log_info("IETM_D0 processed\n");

	carry_capture();
	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *_zone = zones[i].get();
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
	carry_apply();
	thd_log_info("\n\n ZONE DUMP BEGIN\n");
	for (unsigned int i = 0; i < zones.size(); ++i) {
		zones[i]->zone_dump();
	}
	thd_log_info("\n\n ZONE DUMP END\n");
}

/*
 * NotifyPlatform tells the firmware which adaptive target is active by
 * evaluating IETM.IMOK, which the int3400 driver exposes as "imok". Some
 * OEMs (e.g. HP) use it to switch the EC's own thermal mode.
 */
void cthd_engine_adaptive::set_notify_platform(struct adaptive_target &target) {
	csys_fs sysfs("");
	unsigned long value;
	unsigned int argument;

	try {
		value = std::stoul(target.argument, nullptr);
	} catch (const std::invalid_argument &) {
		thd_log_warn("Invalid NotifyPlatform argument: %s\n",
				target.argument.c_str());
		return;
	} catch (const std::out_of_range &) {
		thd_log_warn("NotifyPlatform argument out of range: %s\n",
				target.argument.c_str());
		return;
	}

	/* The int3400 driver parses imok as an unsigned int */
	if (value > UINT_MAX) {
		thd_log_warn("NotifyPlatform argument out of range: %s\n",
				target.argument.c_str());
		return;
	}
	argument = value;

	/*
	 * The int3400 driver only creates imok if the firmware has an IMOK
	 * method. Say so once instead of failing on every target switch.
	 */
	if (!sysfs.exists(int3400_base_path + "imok")) {
		if (!imok_missing_logged) {
			thd_log_info("No %simok, ignoring NotifyPlatform targets\n",
					int3400_base_path.c_str());
			imok_missing_logged = true;
		}
		return;
	}

	thd_log_info("NotifyPlatform %u (target %s)\n", argument,
			target.name.c_str());
	if (sysfs.write(int3400_base_path + "imok", argument) < 0)
		thd_log_warn("Failed to write %u to %simok\n", argument,
				int3400_base_path.c_str());
}

void cthd_engine_adaptive::execute_target(struct adaptive_target &target) {
	cthd_cdev *cdev;
	std::string name;
	int argument;

	thd_log_info("Target Name:%s\n", target.name.c_str());

	if (target.code == "NotifyPlatform") {
		/* NotifyPlatform is an IETM control, evaluated via IETM.IMOK */
		if (target.participant == int3400_path)
			set_notify_platform(target);
		else
			thd_log_info("Ignoring NotifyPlatform for %s, not IETM\n",
					target.participant.c_str());
		return;
	}

	size_t pos = target.participant.find_last_of('.');
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
		argument = std::stoi(target.argument, nullptr);
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

	// A fallback target was chosen at start because no condition set
	// can match: conditions are not evaluated any more, so apply it
	// once here. This must come before the "No change in target"
	// check, as target and current_matched_target are both -1 then.
	if (fallback_id >= 0) {
		if (!policy_active) {
			exec_fallback_target(gddv.targets[fallback_id].target_id);
			policy_active = 1;
		}
		return;
	}

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

int cthd_engine_adaptive::set_int3400_base_path()
{
	const char *base_path = "/sys/bus/platform/drivers/int3400 thermal";
	csys_fs sysfs ("");

	if (sysfs.exists (base_path)) {
		DIR *dir;
		struct dirent *entry;

		if ((dir = opendir (base_path)) != nullptr) {
			while ((entry = readdir (dir)) != nullptr) {
				if (!strncmp (entry->d_name, "INT", strlen ("INT"))) {
					int3400_base_path = "/sys/bus/platform/devices/";
					int3400_base_path += entry->d_name;
					int3400_base_path += "/";
					thd_log_debug ("Discovered int3400 path:%s\n", int3400_base_path.c_str ());
					closedir(dir);
					return THD_SUCCESS;
				}
			}
			closedir(dir);
		}
	}

	return THD_ERROR;
}

int cthd_engine_adaptive::thd_engine_init(bool ignore_cpuid_check,
		bool adaptive) {
	csys_fs sysfs("");
	size_t size;
	int res;

	check_acpi_platform_profile();

	thd_parse_features();

	itmt_pid_config.load();

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

	std::ostringstream filename;
	filename << TDRUNDIR << "/" << "ignore_adaptive";
	csys_fs _sysfs(filename.str().c_str());
	if (_sysfs.exists()) {
		thd_log_info(TDRUNDIR "/ignore_adaptive exists, disabling adaptive\n");
		return THD_ERROR;
	}

	if (set_int3400_base_path() != THD_SUCCESS)
		return THD_ERROR;

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

	std::unique_ptr<char[]> buf(new char[size]);
	if (!buf) {
		thd_log_error("Unable to allocate memory for GDDV");
		return THD_FATAL_ERROR;
	}

	if (sysfs.read(int3400_base_path + "data_vault", buf.get(), size) < int(size)) {
		thd_log_debug("Unable to read GDDV data vault\n");
		return THD_FATAL_ERROR;
	}

	res = gddv.gddv_init(int3400_base_path);
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

	thd_ignore_default_control = 1;

	/* Read the sensors/zones */
	thermal_devices_at_init = thermal_device_count();
	res = cthd_engine::thd_engine_init(ignore_cpuid_check, adaptive);
	if (res != THD_SUCCESS)
		return res;

	/*
	 * Pick a polling table for virtual sensors that have no VSPT of their
	 * own. A VSPT without a participant name (stored as "vsct") is the
	 * shared table. Even when the unnamed participant also has its own
	 * VSCT, its VSPT is still shared with the other sensors, as the old
	 * single-sensor code did. Otherwise, if exactly one VSPT has no
	 * matching VSCT, use that one, so that a table filed under a different
	 * name is not dropped.
	 */
	const std::vector<struct vspt_entry> *shared_vspts = nullptr;
	const std::vector<struct vspt_entry> *orphan_vspts = nullptr;
	int orphans = 0;

	for (unsigned int i = 0; i < gddv.vsensors.size(); ++i) {
		const struct vsensor &vs = gddv.vsensors[i];

		if (!vs.vspts.size())
			continue;

		if (vs.name == "vsct") {
			shared_vspts = &vs.vspts;
		} else if (!vs.vscts.size()) {
			thd_log_info("VSPT [%s] has no matching VSCT\n",
					vs.name.c_str());
			orphan_vspts = &vs.vspts;
			++orphans;
		}
	}

	if (!shared_vspts && orphans == 1)
		shared_vspts = orphan_vspts;

	virt_sensor_names.clear();

	for (unsigned int i = 0; i < gddv.vsensors.size(); ++i) {
		const struct vsensor &vs = gddv.vsensors[i];
		int ret;

		if (!vs.vscts.size())
			continue;

		if (!vs.vspts.size() && shared_vspts)
			ret = install_virtual_sensor(vs, *shared_vspts);
		else
			ret = install_virtual_sensor(vs, vs.vspts);

		if (ret != THD_SUCCESS)
			thd_log_info("Virtual sensor [%s] not installed\n",
					vs.name.c_str());
	}

	return THD_SUCCESS;
}

int cthd_engine_adaptive::install_virtual_sensor(const struct vsensor &vs,
		const std::vector<struct vspt_entry> &vspts) {
	thd_log_info("Found virtual sensor [%s]\n", vs.name.c_str());

	std::string dev_name = vs.name.substr(0, vs.name.find_last_of('.'));

	if (dev_name.empty()) {
		thd_log_info("Can't parse virtual sensor name\n");
		return THD_ERROR;
	}

	/* e.g. VR1.D0 and VR1.D1 would both map to VR1 */
	if (virt_sensor_names.count(dev_name)) {
		thd_log_warn("Virtual sensor [%s]: %s already installed, skipping\n",
				vs.name.c_str(), dev_name.c_str());
		return THD_ERROR;
	}
	virt_sensor_names.insert(dev_name);

	std::string dummy = "";

	std::unique_ptr<cthd_sensor_virtual> virt_sensor(new cthd_sensor_virtual(
		current_sensor_index, dev_name, dummy, 0, 0));

	for (unsigned int i = 0; i < vspts.size(); ++i) {
		struct polling_table_entry entry;

		entry.virtual_temp = vspts[i].virtual_temp;
		entry.sample_period = vspts[i].sample_period;

		virt_sensor->update_polling_table(entry);
	}

//	virt_sensor->enable_periodic_timer();

	for (unsigned int i = 0; i < vs.vscts.size(); ++i) {
		std::string target_name;
		int power_sensor = 0;

		if (vs.vscts[i].coeff_type == 1) {
			target_name = "rapl_pkg_power";
			if (!search_sensor(target_name)){
				std::unique_ptr<cthd_sensor_rapl_power> rapl_power(new cthd_sensor_rapl_power(current_sensor_index));
				if (rapl_power->sensor_update() == THD_SUCCESS) {
					sensors.push_back(std::move(rapl_power));
					++current_sensor_index;
				} else {
					thd_log_info("Can't add power as virtual sensor\n");
					return THD_ERROR;
				}
			}
			power_sensor = 1;
		} else {
			size_t pos = vs.vscts[i].target.find_last_of('.');
			if (pos == std::string::npos)
				target_name = vs.vscts[i].target;
			else
				target_name = vs.vscts[i].target.substr(pos + 1);

			target_name = cpu_participant_name(target_name, true);
		}

		double operation;

		if (vs.vscts[i].operation)
			operation = -1.0;
		else
			operation = 1.0;

		virt_sensor->add_target(target_name, ((double) vs.vscts[i].coeff) / 1000 * operation,
					((double) vs.vscts[i].alpha) / 1000, power_sensor);
	}

	if (virt_sensor->sensor_update() != THD_SUCCESS) {
		thd_log_info("Can't add virtual sensor\n");
		return THD_ERROR;
	}

	sensors.push_back(std::move(virt_sensor));
	++current_sensor_index;

	std::unique_ptr<cthd_zone_dynamic> zone(new cthd_zone_dynamic(current_zone_index,
			dev_name, 0xffffffff, PASSIVE, dev_name, "intel_powerclamp"));
	if (!zone) {
		thd_log_info("Can't add virtual sensor\n");
		return THD_ERROR;
	}

	if (zone->zone_update() != THD_SUCCESS) {
		// sensor will be deleted when all elements of sensors are deleted from sensors[]
		thd_log_info("Can't add virtual sensor\n");
		return THD_ERROR;
	}

	zone->set_zone_active();
	zone->zone_dump();
	zones.push_back(std::move(zone));
	++current_zone_index;

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
	thd_engine.reset(new cthd_engine_adaptive());

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
