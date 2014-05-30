/*
 * thd_engine.cpp: thermal engine class implementation
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

/* This class acts as the parent class of all thermal engines. Main functions are:
 * - Initialization
 * - Read cooling devices and thermal zones(sensors), which can be overridden in child
 * - Starts a poll loop, All the thermal processing happens in this thread's context
 * - Message processing loop
 * - If either a poll interval is expired or notified via netlink, it schedules a
 *   a change notification on the associated cthd_zone to read and process.
 */

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <cpuid.h>
#include <locale>
#include "thd_engine.h"
#include "thd_cdev_therm_sys_fs.h"
#include "thd_zone_therm_sys_fs.h"

static void *cthd_engine_thread(void *arg);

cthd_engine::cthd_engine() :
		cdev_cnt(0), zone_count(0), sensor_count(0), parse_thermal_zone_success(
				false), parse_thermal_cdev_success(false), poll_timeout_msec(
				-1), wakeup_fd(0), control_mode(COMPLEMENTRY), write_pipe_fd(0), preference(
				0), status(true), thz_last_time(0), terminate(false), genuine_intel(
				0), has_invariant_tsc(0), has_aperf(0), proc_list_matched(
				false), poll_interval_sec(0), poll_sensor_mask(0) {
	thd_engine = pthread_t();
	thd_attr = pthread_attr_t();
	thd_cond_var = pthread_cond_t();
	thd_cond_mutex = pthread_mutex_t();
	memset(poll_fds, 0, sizeof(poll_fds));
	memset(last_cpu_update, 0, sizeof(last_cpu_update));
}

cthd_engine::~cthd_engine() {
	unsigned int i;

	for (i = 0; i < sensors.size(); ++i) {
		delete sensors[i];
	}
	sensors.clear();
	for (i = 0; i < zones.size(); ++i) {
		delete zones[i];
	}
	zones.clear();
	for (i = 0; i < cdevs.size(); ++i) {
		delete cdevs[i];
	}
	cdevs.clear();
}

void cthd_engine::thd_engine_thread() {
	unsigned int i;
	int n;

	thd_log_info("thd_engine_thread begin\n");
	for (;;) {
		if (terminate)
			break;

		rapl_power_meter.rapl_measure_power();

		n = poll(poll_fds, THD_NUM_OF_POLL_FDS, poll_timeout_msec);
		thd_log_debug("poll exit %d \n", n);
		if (n < 0) {
			thd_log_warn("Write to pipe failed \n");
			continue;
		}
		if (n == 0) {
			if (!status) {
				thd_log_warn("Thermal Daemon is disabled \n");
				continue;
			}
			// Polling mode enabled. Trigger a temp change message
			for (i = 0; i < zones.size(); ++i) {
				cthd_zone *zone = zones[i];
				zone->zone_temperature_notification(0, 0);
			}
		}
		if (poll_fds[0].revents & POLLIN) {
			// Kobj uevent
			if (kobj_uevent.check_for_event()) {
				time_t tm;

				time(&tm);
				thd_log_debug("kobj uevent for thermal\n");
				if ((tm - thz_last_time) >= thz_notify_debounce_interval) {
					for (i = 0; i < zones.size(); ++i) {
						cthd_zone *zone = zones[i];
						zone->zone_temperature_notification(0, 0);
					}
				} else {
					thd_log_debug("IGNORE THZ kevent\n");
				}
				thz_last_time = tm;
			}
		}
		if (poll_fds[wakeup_fd].revents & POLLIN) {
			message_capsul_t msg;

			thd_log_debug("wakeup fd event\n");
			int result = read(poll_fds[wakeup_fd].fd, &msg,
					sizeof(message_capsul_t));
			if (result < 0) {
				thd_log_warn("read on wakeup fd failed\n");
				poll_fds[wakeup_fd].revents = 0;
				continue;
			}
			if (proc_message(&msg) < 0) {
				thd_log_debug("Terminating thread..\n");
			}
		}
	}
	thd_log_debug("thd_engine_thread_end\n");
}

bool cthd_engine::set_preference(const int pref) {
	return true;
}

int cthd_engine::thd_engine_start(bool ignore_cpuid_check) {
	int ret;
	int wake_fds[2];

	if (ignore_cpuid_check) {
		thd_log_debug("Ignore CPU ID check for MSRs \n");
		proc_list_matched = true;
	} else
		check_cpu_id();

	// Pipe is used for communication between two processes
	ret = pipe(wake_fds);
	if (ret) {
		thd_log_error("Thermal sysfs: pipe creation failed %d:\n", ret);
		return THD_FATAL_ERROR;
	}
	if (fcntl(wake_fds[0], F_SETFL, O_NONBLOCK) < 0) {
		thd_log_error("Cannot set non-blocking on pipe: %s\n", strerror(errno));
		return THD_FATAL_ERROR;
	}
	if (fcntl(wake_fds[1], F_SETFL, O_NONBLOCK) < 0) {
		thd_log_error("Cannot set non-blocking on pipe: %s\n", strerror(errno));
		return THD_FATAL_ERROR;
	}
	write_pipe_fd = wake_fds[1];
	wakeup_fd = THD_NUM_OF_POLL_FDS - 1;

	memset(poll_fds, 0, sizeof(poll_fds));
	poll_fds[wakeup_fd].fd = wake_fds[0];
	poll_fds[wakeup_fd].events = POLLIN;
	poll_fds[wakeup_fd].revents = 0;

	poll_timeout_msec = -1;
	if (poll_interval_sec) {
		thd_log_warn("Polling mode is enabled: %d\n", poll_interval_sec);
		poll_timeout_msec = poll_interval_sec * 1000;
	}

	ret = read_thermal_sensors();
	if (ret != THD_SUCCESS) {
		thd_log_error("Thermal sysfs Error in reading sensors\n");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	ret = read_cooling_devices();
	if (ret != THD_SUCCESS) {
		thd_log_error("Thermal sysfs Error in reading cooling devs\n");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	ret = read_thermal_zones();
	if (ret != THD_SUCCESS) {
		thd_log_error("No thermal sensors found\n");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	// Check if polling is disabled and sensors don't support
	// async mode, in that enable force polling
	if (!poll_interval_sec) {
		unsigned int i;
		for (i = 0; i < zones.size(); ++i) {
			cthd_zone *zone = zones[i];
			if (!zone->zone_active_status())
				continue;
			if (!zone->check_sensor_async_status()) {
				thd_log_warn(
						"Polling will be enabled as some sensors are not capable to notify asynchnously \n");
				poll_timeout_msec = def_poll_interval;
				break;
			}
		}
		if (i == zones.size()) {
			thd_log_info("Proceed without polling mode! \n");
		}
	}

	poll_fds[0].fd = kobj_uevent.kobj_uevent_open();
	if (poll_fds[0].fd < 0) {
		thd_log_warn("Invalid kobj_uevent handle\n");
		goto skip_kobj;
	}
	thd_log_info("FD = %d\n", poll_fds[0].fd);
	kobj_uevent.register_dev_path(
			(char *) "/devices/virtual/thermal/thermal_zone");
	poll_fds[0].events = POLLIN;
	poll_fds[0].revents = 0;
	skip_kobj:
#ifndef DISABLE_PTHREAD
	// condition variable
	pthread_cond_init(&thd_cond_var, NULL);
	pthread_mutex_init(&thd_cond_mutex, NULL);
	// Create thread
	pthread_attr_init(&thd_attr);
	pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&thd_engine, &thd_attr, cthd_engine_thread,
			(void*) this);
#else
	{
		pid_t childpid;

		if((childpid = fork()) == - 1)
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if(childpid == 0)
		{
			/* Child process closes up input side of pipe */
			close(wake_fds[1]);
			cthd_engine_thread((void*)this);
		}
		else
		{
			/* Parent process closes up output side of pipe */
			close(wake_fds[0]);
		}
	}
#endif
	preference = thd_pref.get_preference();
	thd_log_info("Current user preference is %d\n", preference);

	if (control_mode == EXCLUSIVE) {
		thd_log_info("Control is taken over from kernel\n");
		takeover_thermal_control();
	}
	return ret;
}

int cthd_engine::thd_engine_stop() {
	return THD_SUCCESS;
}

static void *cthd_engine_thread(void *arg) {
	cthd_engine *obj = (cthd_engine*) arg;

	obj->thd_engine_thread();

	return NULL;
}

void cthd_engine::send_message(message_name_t msg_id, int size,
		unsigned char *msg) {
	message_capsul_t msg_cap;

	memset(&msg_cap, 0, sizeof(message_capsul_t));

	msg_cap.msg_id = msg_id;
	msg_cap.msg_size = (size > MAX_MSG_SIZE) ? MAX_MSG_SIZE : size;
	if (msg)
		memcpy(msg_cap.msg, msg, msg_cap.msg_size);
	int result = write(write_pipe_fd, &msg_cap, sizeof(message_capsul_t));
	if (result < 0)
		thd_log_warn("Write to pipe failed \n");
}

void cthd_engine::process_pref_change() {
	int new_pref;

	thd_pref.refresh();
	new_pref = thd_pref.get_preference();
	if (new_pref == PREF_DISABLED) {
		status = false;
		return;
	}
	status = true;
	if (preference != new_pref) {
		thd_log_warn("Preference changed \n");
	}
	preference = new_pref;
	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *zone = zones[i];
		zone->update_zone_preference();
	}

	if (control_mode == EXCLUSIVE) {
		thd_log_info("Control is taken over from kernel\n");
		takeover_thermal_control();
	}
}

void cthd_engine::thd_engine_terminate() {
	send_message(TERMINATE, 0, NULL);
	sleep(1);
	process_terminate();
}

int cthd_engine::thd_engine_set_user_max_temp(const char *zone_type,
		const char *user_set_point) {
	std::string str(user_set_point);
	cthd_zone *zone;

	thd_log_debug("thd_engine_set_user_set_point %s\n", user_set_point);

	std::locale loc;
	if (std::isdigit(str[0], loc) == 0) {
		thd_log_warn("thd_engine_set_user_set_point Invalid set point\n");
		return THD_ERROR;
	}

	zone = get_zone(zone_type);
	if (!zone) {
		thd_log_warn("thd_engine_set_user_set_point Invalid zone\n");
		return THD_ERROR;
	}
	return zone->update_max_temperature(atoi(user_set_point));
}

int cthd_engine::thd_engine_set_user_psv_temp(const char *zone_type,
		const char *user_set_point) {
	std::string str(user_set_point);
	cthd_zone *zone;

	thd_log_debug("thd_engine_set_user_psv_temp %s\n", user_set_point);

	std::locale loc;
	if (std::isdigit(str[0], loc) == 0) {
		thd_log_warn("thd_engine_set_user_psv_temp Invalid set point\n");
		return THD_ERROR;
	}

	zone = get_zone(zone_type);
	if (!zone) {
		thd_log_warn("thd_engine_set_user_psv_temp Invalid zone\n");
		return THD_ERROR;
	}
	return zone->update_psv_temperature(atoi(user_set_point));
}

void cthd_engine::thermal_zone_change(message_capsul_t *msg) {

	thermal_zone_notify_t *pmsg = (thermal_zone_notify_t*) msg->msg;
	for (unsigned i = 0; i < zones.size(); ++i) {
		cthd_zone *zone = zones[i];
		if (zone->zone_active_status())
			zone->zone_temperature_notification(pmsg->type, pmsg->data);
		else {
			thd_log_debug("zone is not active\n");
		}
	}
}

void cthd_engine::poll_enable_disable(bool status, message_capsul_t *msg) {
	unsigned int *sensor_id = (unsigned int*) msg->msg;

	if (status) {
		poll_sensor_mask |= (1 << (*sensor_id));
		poll_timeout_msec = def_poll_interval;
		thd_log_debug("thd_engine polling enabled via %u \n", *sensor_id);
	} else {
		poll_sensor_mask &= ~(1 << (*sensor_id));
		if (!poll_sensor_mask) {
			poll_timeout_msec = -1;
			thd_log_debug("thd_engine polling last disabled via %u \n",
					*sensor_id);
		}
	}
}

int cthd_engine::proc_message(message_capsul_t *msg) {
	int ret = 0;

	thd_log_debug("Receieved message %d\n", msg->msg_id);
	switch (msg->msg_id) {
	case WAKEUP:
		break;
	case TERMINATE:
		thd_log_warn("Terminating ...\n");

		ret = -1;
		terminate = true;
		break;
	case PREF_CHANGED:
		process_pref_change();
		break;
	case THERMAL_ZONE_NOTIFY:
		if (!status) {
			thd_log_warn("Thermal Daemon is disabled \n");
			break;
		}
		thermal_zone_change(msg);
		break;
	case CALIBRATE: {
		//TO DO
	}
		break;
	case RELOAD_ZONES:
		thd_engine_reload_zones();
		break;
	case POLL_ENABLE:
		if (!poll_interval_sec) {
			poll_enable_disable(true, msg);
		}
		break;
	case POLL_DISABLE:
		if (!poll_interval_sec) {
			poll_enable_disable(false, msg);
		}
		break;
	default:
		break;
	}

	return ret;
}

cthd_cdev *cthd_engine::thd_get_cdev_at_index(int index) {
	for (int i = 0; i < cdev_cnt; ++i) {
		if (cdevs[i]->thd_cdev_get_index() == index)
			return cdevs[i];
	}
	return NULL;
}

void cthd_engine::takeover_thermal_control() {
	csys_fs sysfs("/sys/class/thermal/");

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";

	thd_log_info("Taking over thermal control \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;

				i = atoi(entry->d_name + strlen("thermal_zone"));
				std::stringstream policy;
				std::string curr_policy;

				policy << "thermal_zone" << i << "/policy";
				if (sysfs.exists(policy.str().c_str())) {
					sysfs.read(policy.str(), curr_policy);
					zone_preferences.push_back(curr_policy);
					sysfs.write(policy.str(), "user_space");
				}
			}
		}
		closedir(dir);
	}
}

void cthd_engine::giveup_thermal_control() {
	if (control_mode != EXCLUSIVE)
		return;

	if (zone_preferences.size() == 0)
		return;

	thd_log_info("Giving up thermal control \n");

	csys_fs sysfs("/sys/class/thermal/");

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int cnt = 0;
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;

				i = atoi(entry->d_name + strlen("thermal_zone"));
				std::stringstream policy;

				policy << "thermal_zone" << i << "/policy";
				if (sysfs.exists(policy.str().c_str())) {
					sysfs.write(policy.str(), zone_preferences[cnt++]);
				}
			}
		}
		closedir(dir);
	}
}

void cthd_engine::process_terminate() {
	thd_log_warn("terminating on user request ..\n");
	giveup_thermal_control();
	exit(EXIT_SUCCESS);
}

void cthd_engine::thd_engine_poll_enable(int sensor_id) {
	send_message(POLL_ENABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_poll_disable(int sensor_id) {
	send_message(POLL_DISABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_reload_zones() {
	thd_log_warn(" Reloading zones\n");
	for (unsigned int i = 0; i < zones.size(); ++i) {
		cthd_zone *zone = zones[i];
		delete zone;
	}
	zones.clear();

	int ret = read_thermal_zones();
	if (ret != THD_SUCCESS) {
		thd_log_error("No thermal sensors found\n");
		// This is a fatal error and daemon will exit
		return;
	}
}

// Add any tested platform ids in this table
static supported_ids_t id_table[] = { { 6, 0x2a }, // Sandybridge
		{ 6, 0x2d }, // Sandybridge
		{ 6, 0x3a }, // IvyBridge
		{ 6, 0x3c }, { 6, 0x3e }, { 6, 0x3f }, { 6, 0x45 }, // Haswell ULT */
		{ 6, 0x46 }, // Haswell ULT */

		{ 0, 0 } // Last Invalid entry
};

int cthd_engine::check_cpu_id() {
#ifndef ANDROID
	// Copied from turbostat program
	unsigned int ebx, ecx, edx, max_level;
	unsigned int fms, family, model, stepping;
	genuine_intel = 0;
	int i = 0;
	bool valid = false;

	proc_list_matched = false;
	ebx = ecx = edx = 0;

	__cpuid(0, max_level, ebx, ecx, edx);
	if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
		genuine_intel = 1;
	if (genuine_intel == 0) {
		// Simply return without further capability check
		return THD_SUCCESS;
	}
	__cpuid(1, fms, ebx, ecx, edx);
	family = (fms >> 8) & 0xf;
	model = (fms >> 4) & 0xf;
	stepping = fms & 0xf;
	if (family == 6 || family == 0xf)
		model += ((fms >> 16) & 0xf) << 4;

	thd_log_warn(
			"%u CPUID levels; family:model:stepping 0x%x:%x:%x (%u:%u:%u)\n",
			max_level, family, model, stepping, family, model, stepping);

	while (id_table[i].family) {
		if (id_table[i].family == family && id_table[i].model == model) {
			proc_list_matched = true;
			valid = true;
			break;
		}
		i++;
	}
	if (!valid) {
		thd_log_warn(" Need Linux PowerCap sysfs \n");
	}

#endif
	return THD_SUCCESS;
}

void cthd_engine::thd_read_default_thermal_sensors() {
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";

	thd_log_debug("thd_read_default_thermal_sensors \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;
				i = atoi(entry->d_name + strlen("thermal_zone"));
				cthd_sensor *sensor = new cthd_sensor(i,
						base_path + entry->d_name + "/", "");
				if (sensor->sensor_update() != THD_SUCCESS) {
					delete sensor;
					continue;
				}
				sensors.push_back(sensor);
			}
		}
		closedir(dir);
	}
	sensor_count = sensors.size();
	thd_log_info("thd_read_default_thermal_sensors loaded %d sensors \n",
			sensor_count);
}

void cthd_engine::thd_read_default_thermal_zones() {
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";

	thd_log_debug("thd_read_default_thermal_zones \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;
				i = atoi(entry->d_name + strlen("thermal_zone"));
				cthd_sysfs_zone *zone = new cthd_sysfs_zone(i,
						"/sys/class/thermal/thermal_zone");
				if (zone->zone_update() != THD_SUCCESS) {
					delete zone;
					continue;
				}
				if (control_mode == EXCLUSIVE)
					zone->set_zone_active();
				zones.push_back(zone);
			}
		}
		closedir(dir);
	}
	zone_count = zones.size();
	thd_log_info("thd_read_default_thermal_zones loaded %d zones \n",
			zone_count);
}

void cthd_engine::thd_read_default_cooling_devices() {

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";

	thd_log_debug("thd_read_default_cooling devices \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "cooling_device",
					strlen("cooling_device"))) {
				int i;
				i = atoi(entry->d_name + strlen("cooling_device"));
				cthd_sysfs_cdev *cdev = new cthd_sysfs_cdev(i,
						"/sys/class/thermal/");
				if (cdev->update() != THD_SUCCESS) {
					delete cdev;
					continue;
				}
				cdevs.push_back(cdev);
			}
		}
		closedir(dir);
	}
	cdev_cnt = cdevs.size();
	thd_log_info("thd_read_default_cooling devices loaded %d cdevs \n",
			cdev_cnt);
}

cthd_zone* cthd_engine::search_zone(std::string name) {
	cthd_zone *zone;

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zone = zones[i];
		if (!zone)
			continue;
		if (zone->get_zone_type() == name)
			return zone;
	}

	return NULL;
}

cthd_cdev* cthd_engine::search_cdev(std::string name) {
	cthd_cdev *cdev;

	for (unsigned int i = 0; i < cdevs.size(); ++i) {
		cdev = cdevs[i];
		if (!cdev)
			continue;
		if (cdev->get_cdev_type() == name)
			return cdev;
	}

	return NULL;
}

cthd_sensor* cthd_engine::search_sensor(std::string name) {
	cthd_sensor *sensor;

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		sensor = sensors[i];
		if (!sensor)
			continue;
		if (sensor->get_sensor_type() == name)
			return sensor;
	}

	return NULL;
}

cthd_sensor* cthd_engine::get_sensor(int index) {
	if (index >= 0 && index < (int) sensors.size())
		return sensors[index];
	else
		return NULL;
}

cthd_zone* cthd_engine::get_zone(int index) {
	if (index == -1)
		return NULL;
	if (index >= 0 && index < (int) zones.size())
		return zones[index];
	else
		return NULL;
}

cthd_zone* cthd_engine::get_zone(std::string type) {
	cthd_zone *zone;

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zone = zones[i];
		if (zone->get_zone_type() == type)
			return zone;
	}

	return NULL;
}
