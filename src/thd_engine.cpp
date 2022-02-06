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
#include <sys/utsname.h>
#include <cpuid.h>
#include <locale>
#include "thd_engine.h"
#include "thd_cdev_therm_sys_fs.h"
#include "thd_zone_therm_sys_fs.h"
#include "thd_zone_dynamic.h"
#include "thd_cdev_gen_sysfs.h"
#include "thd_int3400.h"

static void *cthd_engine_thread(void *arg);

cthd_engine::cthd_engine(std::string _uuid) :
		current_cdev_index(0), current_zone_index(0), current_sensor_index(0), parse_thermal_zone_success(
				false), parse_thermal_cdev_success(false), uuid(_uuid), parser_disabled(
				false), adaptive_mode(false), poll_timeout_msec(-1), wakeup_fd(
				-1), uevent_fd(-1), control_mode(COMPLEMENTRY), write_pipe_fd(
				0), preference(0), status(true), thz_last_uevent_time(0), thz_last_temp_ind_time(
				0), thz_last_update_event_time(0), terminate(false), genuine_intel(
				0), has_invariant_tsc(0), has_aperf(0), proc_list_matched(
				false), poll_interval_sec(0), poll_sensor_mask(0), fast_poll_sensor_mask(
				0), saved_poll_interval(0), poll_fd_cnt(0), rt_kernel(false), parser_init_done(
				false) {
	thd_engine = pthread_t();
	thd_attr = pthread_attr_t();

	pthread_mutex_init(&thd_engine_mutex, NULL);

	memset(poll_fds, 0, sizeof(poll_fds));
	memset(last_cpu_update, 0, sizeof(last_cpu_update));
}

cthd_engine::~cthd_engine() {
	unsigned int i;

	if (parser_init_done)
		parser.parser_deinit();

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
	time_t tm;
	int poll_timeout_sec = poll_timeout_msec / 1000;

	thd_log_info("thd_engine_thread begin\n");
	for (;;) {
		if (terminate)
			break;

		n = poll(poll_fds, poll_fd_cnt, poll_timeout_msec);
		thd_log_debug("poll exit %d polls_fd event %d %d\n", n,
				poll_fds[0].revents, poll_fds[1].revents);
		if (n < 0) {
			thd_log_warn("Write to pipe failed \n");
			continue;
		}
		time(&tm);
		rapl_power_meter.rapl_measure_power();

		if (n == 0 || (tm - thz_last_temp_ind_time) >= poll_timeout_sec) {
			if (!status) {
				thd_log_msg("Thermal Daemon is disabled \n");
				continue;
			}
			pthread_mutex_lock(&thd_engine_mutex);
			// Polling mode enabled. Trigger a temp change message
			for (i = 0; i < zones.size(); ++i) {
				cthd_zone *zone = zones[i];
				zone->zone_temperature_notification(0, 0);
			}
			pthread_mutex_unlock(&thd_engine_mutex);
			thz_last_temp_ind_time = tm;
		}
		if (uevent_fd >= 0 && (poll_fds[uevent_fd].revents & POLLIN)) {
			// Kobj uevent
			if (kobj_uevent.check_for_event()) {
				time_t tm;

				time(&tm);
				thd_log_debug("kobj uevent for thermal\n");
				if ((tm - thz_last_uevent_time)
						>= thz_notify_debounce_interval) {
					pthread_mutex_lock(&thd_engine_mutex);
					for (i = 0; i < zones.size(); ++i) {
						cthd_zone *zone = zones[i];
						zone->zone_temperature_notification(0, 0);
					}
					pthread_mutex_unlock(&thd_engine_mutex);
				} else {
					thd_log_debug("IGNORE THZ kevent\n");
				}
				thz_last_uevent_time = tm;
			}
		}
		if (wakeup_fd >= 0 && (poll_fds[wakeup_fd].revents & POLLIN)) {
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

		if ((tm - thz_last_update_event_time) >= thd_poll_interval) {
			pthread_mutex_lock(&thd_engine_mutex);
			update_engine_state();
			pthread_mutex_unlock(&thd_engine_mutex);
			thz_last_update_event_time = tm;
		}

		workarounds();
	}
	thd_log_debug("thd_engine_thread_end\n");
}

bool cthd_engine::set_preference(const int pref) {
	return true;
}

int cthd_engine::thd_engine_init(bool ignore_cpuid_check, bool adaptive) {
	int ret;

	adaptive_mode = adaptive;

	if (ignore_cpuid_check) {
		thd_log_debug("Ignore CPU ID check for MSRs \n");
		proc_list_matched = true;
	} else {
		check_cpu_id();

		if (!proc_list_matched) {
			if ((parser_init() == THD_SUCCESS) && parser.platform_matched()) {
				thd_log_msg("Unsupported cpu model, using thermal-conf.xml only \n");
			} else {
				thd_log_msg("Unsupported cpu model, use thermal-conf.xml file or run with --ignore-cpuid-check \n");
				return THD_ERROR;
			}
		}
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

	return THD_SUCCESS;
}

int cthd_engine::thd_engine_start() {
	int ret;
	int wake_fds[2];

	check_for_rt_kernel();

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

	memset(poll_fds, 0, sizeof(poll_fds));

	wakeup_fd = poll_fd_cnt;
	poll_fds[wakeup_fd].fd = wake_fds[0];
	poll_fds[wakeup_fd].events = POLLIN;
	poll_fds[wakeup_fd].revents = 0;
	poll_fd_cnt++;

	poll_timeout_msec = -1;
	if (poll_interval_sec) {
		thd_log_msg("Polling mode is enabled: %d\n", poll_interval_sec);
		poll_timeout_msec = poll_interval_sec * 1000;
	}

	if (parser.platform_matched()) {
		parser.set_default_preference();
		int poll_secs = parser.get_polling_interval();
		if (poll_secs) {
			thd_log_info("Poll interval is defined in XML config %d seconds\n", poll_secs);
			poll_interval_sec = poll_secs;
			poll_timeout_msec = poll_secs * 1000;
		}
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
				thd_log_msg(
						"Polling will be enabled as some sensors are not capable to notify asynchronously\n");
				poll_timeout_msec = def_poll_interval;
				break;
			}
		}
		if (i == zones.size()) {
			thd_log_info("Proceed without polling mode!\n");
		}

		uevent_fd = poll_fd_cnt;
		poll_fds[uevent_fd].fd = kobj_uevent.kobj_uevent_open();
		if (poll_fds[uevent_fd].fd < 0) {
			thd_log_warn("Invalid kobj_uevent handle\n");
			uevent_fd = -1;
			goto skip_kobj;
		}
		thd_log_info("FD = %d\n", poll_fds[uevent_fd].fd);
		kobj_uevent.register_dev_path(
				(char *) "/devices/virtual/thermal/thermal_zone");
		poll_fds[uevent_fd].events = POLLIN;
		poll_fds[uevent_fd].revents = 0;
		poll_fd_cnt++;
	}
	skip_kobj:
#ifndef DISABLE_PTHREAD
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
	thd_pref.refresh();
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
		thd_log_msg("Preference changed \n");
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

void cthd_engine::fast_poll_enable_disable(bool status, message_capsul_t *msg) {
	unsigned int *sensor_id = (unsigned int*) msg->msg;

	if (status) {
		fast_poll_sensor_mask |= (1 << (*sensor_id));
		if (!saved_poll_interval)
			saved_poll_interval = poll_timeout_msec;
		poll_timeout_msec = 1000;
		thd_log_debug("thd_engine fast polling enabled via %u \n", *sensor_id);
	} else {
		fast_poll_sensor_mask &= ~(1 << (*sensor_id));
		if (!fast_poll_sensor_mask) {
			if (saved_poll_interval)
				poll_timeout_msec = saved_poll_interval;
			thd_log_debug("thd_engine polling last disabled via %u \n",
					*sensor_id);
		}
	}
}

int cthd_engine::proc_message(message_capsul_t *msg) {
	int ret = 0;

	thd_log_debug("Received message %d\n", msg->msg_id);
	switch (msg->msg_id) {
	case WAKEUP:
		break;
	case TERMINATE:
		thd_log_msg("Terminating ...\n");

		ret = -1;
		terminate = true;
		break;
	case PREF_CHANGED:
		process_pref_change();
		break;
	case THERMAL_ZONE_NOTIFY:
		if (!status) {
			thd_log_msg("Thermal Daemon is disabled \n");
			break;
		}
		thermal_zone_change(msg);
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
	case FAST_POLL_ENABLE:
		fast_poll_enable_disable(true, msg);
		break;
	case FAST_POLL_DISABLE:
		fast_poll_enable_disable(false, msg);
		break;
	default:
		break;
	}

	return ret;
}

cthd_cdev *cthd_engine::thd_get_cdev_at_index(int index) {
	for (int i = 0; i < (int) cdevs.size(); ++i) {
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
	cthd_INT3400 int3400(uuid);

	thd_log_info("Taking over thermal control \n");

	int3400.set_default_uuid();

	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;

				i = atoi(entry->d_name + strlen("thermal_zone"));
				std::stringstream policy;
				std::string curr_policy;
				std::stringstream type;
				std::string thermal_type;
				std::stringstream mode;

				policy << "thermal_zone" << i << "/policy";
				if (sysfs.exists(policy.str().c_str())) {
					int ret;

					ret = sysfs.read(policy.str(), curr_policy);
					if (ret >= 0) {
						zone_preferences.push_back(curr_policy);
						sysfs.write(policy.str(), "user_space");
					}
				}
				type << "thermal_zone" << i << "/type";
				if (sysfs.exists(type.str().c_str())) {
					sysfs.read(type.str(), thermal_type);
					thd_log_info("Thermal zone of type %s\n", thermal_type.c_str());
					if (thermal_type == "INT3400") {
						mode << "thermal_zone" << i << "/mode";
						sysfs.write(mode.str(), "enabled");
					}
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
				std::stringstream type;
				std::string thermal_type;
				std::stringstream mode;

				policy << "thermal_zone" << i << "/policy";
				if (sysfs.exists(policy.str().c_str())) {
					sysfs.write(policy.str(), zone_preferences[cnt++]);
				}
				type << "thermal_zone" << i << "/type";
				if (sysfs.exists(type.str().c_str())) {
					sysfs.read(type.str(), thermal_type);
					if (thermal_type == "INT3400") {
						mode << "thermal_zone" << i << "/mode";
						sysfs.write(mode.str(), "disabled");
					}
				}
			}
		}
		closedir(dir);
	}
}

void cthd_engine::process_terminate() {
	thd_log_msg("terminating on user request ..\n");
	giveup_thermal_control();
}

void cthd_engine::thd_engine_poll_enable(int sensor_id) {
	send_message(POLL_ENABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_poll_disable(int sensor_id) {
	send_message(POLL_DISABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_fast_poll_enable(int sensor_id) {
	send_message(FAST_POLL_ENABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_fast_poll_disable(int sensor_id) {
	send_message(FAST_POLL_DISABLE, (int) sizeof(sensor_id),
			(unsigned char*) &sensor_id);
}

void cthd_engine::thd_engine_reload_zones() {
	thd_log_msg(" Reloading zones\n");
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
#ifndef ANDROID
static supported_ids_t id_table[] = {
		{ 6, 0x2a }, // Sandybridge
		{ 6, 0x3a }, // IvyBridge
		{ 6, 0x3c }, // Haswell
		{ 6, 0x45 }, // Haswell ULT
		{ 6, 0x46 }, // Haswell ULT
		{ 6, 0x3d }, // Broadwell
		{ 6, 0x47 }, // Broadwell-GT3E
		{ 6, 0x37 }, // Valleyview BYT
		{ 6, 0x4c }, // Brasewell
		{ 6, 0x4e }, // skylake
		{ 6, 0x5e }, // skylake
		{ 6, 0x5c }, // Broxton
		{ 6, 0x7a }, // Gemini Lake
		{ 6, 0x8e }, // kabylake
		{ 6, 0x9e }, // kabylake
		{ 6, 0x66 }, // Cannonlake
		{ 6, 0x7e }, // Icelake
		{ 6, 0x8c }, // Tigerlake_L
		{ 6, 0x8d }, // Tigerlake
		{ 6, 0xa5 }, // Cometlake
		{ 6, 0xa6 }, // Cometlake_L
		{ 6, 0xa7 }, // Rocketlake
		{ 6, 0x9c }, // Jasper Lake
		{ 6, 0x97 }, // Alderlake
		{ 6, 0x9a }, // Alderlake
		{ 0, 0 } // Last Invalid entry
};

std::vector<std::string> blocklist_paths {
	/* Some Lenovo machines have in-firmware thermal management,
	 * avoid having two entities trying to manage things.
	 * We may want to change this to dytc_perfmode once that is
	 * widely available. */
	"/sys/devices/platform/thinkpad_acpi/dytc_lapmode",
};
#endif

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

	thd_log_msg(
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
		thd_log_msg(" Need Linux PowerCap sysfs \n");
	}


	for (std::string path : blocklist_paths) {
		struct stat s;

		if (!stat(path.c_str(), &s)) {
			proc_list_matched = false;
			thd_log_warn("[%s] present: Thermald can't run on this platform\n", path.c_str());
			break;
		}
	}

#endif
	return THD_SUCCESS;
}

void cthd_engine::thd_read_default_thermal_sensors() {
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int max_index = 0;

	if ((dir = opendir("/sys/class/thermal/thermal_zone1/")) == NULL) {
		thd_log_info("Waiting for thermal sysfs to be ready\n");
		sleep(2);
	} else {
		closedir(dir);
	}

	thd_log_debug("thd_read_default_thermal_sensors \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;
				i = atoi(entry->d_name + strlen("thermal_zone"));
				if (i > max_index)
					max_index = i;
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
	if (sensors.size())
		current_sensor_index = max_index + 1;

	thd_log_info("thd_read_default_thermal_sensors loaded %zu sensors \n",
			sensors.size());
}

void cthd_engine::thd_read_default_thermal_zones() {
	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int max_index = 0;

	thd_log_debug("thd_read_default_thermal_zones \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "thermal_zone",
					strlen("thermal_zone"))) {
				int i;
				i = atoi(entry->d_name + strlen("thermal_zone"));
				if (i > max_index)
					max_index = i;
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
	if (zones.size())
		current_zone_index = max_index + 1;
	thd_log_info("thd_read_default_thermal_zones loaded %zu zones \n",
			zones.size());
}

void cthd_engine::thd_read_default_cooling_devices() {

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int max_index = 0;

	thd_log_debug("thd_read_default_cooling devices \n");
	if ((dir = opendir(base_path.c_str())) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (!strncmp(entry->d_name, "cooling_device",
					strlen("cooling_device"))) {
				int i;
				i = atoi(entry->d_name + strlen("cooling_device"));
				if (i > max_index)
					max_index = i;
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
	if (cdevs.size())
		current_cdev_index = max_index + 1;

	thd_log_info("thd_read_default_cooling devices loaded %zu cdevs \n",
			cdevs.size());
}

ppcc_t* cthd_engine::get_ppcc_param(std::string name) {
	return parser.get_ppcc_param(name);
}

cthd_zone* cthd_engine::search_zone(std::string name) {
	cthd_zone *zone;

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zone = zones[i];
		if (!zone)
			continue;
		if (zone->get_zone_type() == name)
			return zone;
		if (!name.compare(0, 4, "pch_")) {
			if (!zone->get_zone_type().compare(0, 4, "pch_")) {
				thd_log_info("Matching partial zone %s %s\n", name.c_str(),
						zone->get_zone_type().c_str());
				return zone;
			}
		}
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
		if (cdev->get_cdev_alias() == name)
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
		if (!name.compare(0, 4, "pch_")) {
			if (!sensor->get_sensor_type().compare(0, 4, "pch_")) {
				thd_log_info("Matching partial %s %s\n", name.c_str(),
						sensor->get_sensor_type().c_str());
				return sensor;
			}
		}

	}

	return NULL;
}

cthd_sensor* cthd_engine::get_sensor(int index) {
	if (index >= 0 && index < (int) sensors.size())
		return sensors[index];
	else
		return NULL;
}

int cthd_engine::get_sensor_temperature(int index, unsigned int *temperature) {
	if (index >= 0 && index < (int) sensors.size()) {
		*temperature = sensors[index]->read_temperature();
		return THD_SUCCESS;
	} else
		return THD_ERROR;
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

// Code copied from
// https://rt.wiki.kernel.org/index.php/RT_PREEMPT_HOWTO#Runtime_detection_of_an_RT-PREEMPT_Kernel
void cthd_engine::check_for_rt_kernel() {
	struct utsname _uname;
	char *crit1 = NULL;
	int crit2 = 0;
	FILE *fd;

	uname(&_uname);
	crit1 = strcasestr(_uname.version, "PREEMPT RT");

	if ((fd = fopen("/sys/kernel/realtime", "r")) != NULL) {
		int flag;
		crit2 = ((fscanf(fd, "%d", &flag) == 1) && (flag == 1));
		fclose(fd);
	}
	if (crit1 && crit2)
		rt_kernel = true;
	else
		rt_kernel = false;

	thd_log_info("Running on a %s kernel\n",
			rt_kernel ? "PREEMPT RT" : "vanilla");
}

int cthd_engine::user_add_sensor(std::string name, std::string path) {
	cthd_sensor *sensor;

	pthread_mutex_lock(&thd_engine_mutex);

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		if (sensors[i]->get_sensor_type() == name) {
			sensor = sensors[i];
			sensor->update_path(path);
			pthread_mutex_unlock(&thd_engine_mutex);
			return THD_SUCCESS;
		}
	}
	sensor = new cthd_sensor(current_sensor_index, path, name, SENSOR_TYPE_RAW);
	if (sensor->sensor_update() != THD_SUCCESS) {
		delete sensor;
		pthread_mutex_unlock(&thd_engine_mutex);
		return THD_ERROR;
	}
	sensors.push_back(sensor);
	++current_sensor_index;
	pthread_mutex_unlock(&thd_engine_mutex);

	send_message(WAKEUP, 0, 0);

	return THD_SUCCESS;
}

int cthd_engine::user_add_virtual_sensor(std::string name,
		std::string dep_sensor, double slope, double intercept) {
	cthd_sensor *sensor;
	int ret;

	pthread_mutex_lock(&thd_engine_mutex);

	for (unsigned int i = 0; i < sensors.size(); ++i) {
		if (sensors[i]->get_sensor_type() == name) {
			sensor = sensors[i];
			if (sensor->is_virtual()) {
				cthd_sensor_virtual *virt_sensor =
						(cthd_sensor_virtual *) sensor;
				ret = virt_sensor->sensor_update_param(dep_sensor, slope,
						intercept);
			} else {
				pthread_mutex_unlock(&thd_engine_mutex);
				return THD_ERROR;
			}
			pthread_mutex_unlock(&thd_engine_mutex);
			return ret;
		}
	}
	cthd_sensor_virtual *virt_sensor;

	virt_sensor = new cthd_sensor_virtual(current_sensor_index, name,
			dep_sensor, slope, intercept);
	if (virt_sensor->sensor_update() != THD_SUCCESS) {
		delete virt_sensor;
		pthread_mutex_unlock(&thd_engine_mutex);
		return THD_ERROR;
	}
	sensors.push_back(virt_sensor);
	++current_sensor_index;
	pthread_mutex_unlock(&thd_engine_mutex);

	send_message(WAKEUP, 0, 0);

	return THD_SUCCESS;
}

cthd_sensor *cthd_engine::user_get_sensor(unsigned int index) {

	if (index < sensors.size())
		return sensors[index];
	else
		return NULL;
}

cthd_zone *cthd_engine::user_get_zone(unsigned int index) {
	if (index < zones.size())
		return zones[index];
	else
		return NULL;
}

cthd_cdev *cthd_engine::user_get_cdev(unsigned int index) {
	if (index < cdevs.size())
		return cdevs[index];
	else
		return NULL;
}

int cthd_engine::user_set_psv_temp(std::string name, unsigned int temp) {
	cthd_zone *zone;
	int ret;

	pthread_mutex_lock(&thd_engine_mutex);
	zone = get_zone(name);
	if (!zone) {
		pthread_mutex_unlock(&thd_engine_mutex);
		thd_log_warn("user_set_psv_temp\n");
		return THD_ERROR;
	}
	thd_log_info("Setting psv %u\n", temp);
	ret = zone->update_psv_temperature(temp);
	pthread_mutex_unlock(&thd_engine_mutex);

	return ret;
}

int cthd_engine::user_set_max_temp(std::string name, unsigned int temp) {
	cthd_zone *zone;
	int ret;

	pthread_mutex_lock(&thd_engine_mutex);
	zone = get_zone(name);
	if (!zone) {
		pthread_mutex_unlock(&thd_engine_mutex);
		thd_log_warn("user_set_max_temp\n");
		return THD_ERROR;
	}
	thd_log_info("Setting max %u\n", temp);
	ret = zone->update_max_temperature(temp);
	pthread_mutex_unlock(&thd_engine_mutex);

	return ret;
}

int cthd_engine::user_add_zone(std::string zone_name, unsigned int trip_temp,
		std::string sensor_name, std::string cdev_name) {
	int ret = THD_SUCCESS;

	cthd_zone_dynamic *zone = new cthd_zone_dynamic(current_zone_index,
			zone_name, trip_temp, PASSIVE, sensor_name, cdev_name);
	if (!zone) {
		return THD_ERROR;
	}
	if (zone->zone_update() == THD_SUCCESS) {
		pthread_mutex_lock(&thd_engine_mutex);
		zones.push_back(zone);
		pthread_mutex_unlock(&thd_engine_mutex);
		zone->set_zone_active();
		++current_zone_index;
	} else {
		delete zone;
		return THD_ERROR;
	}

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zones[i]->zone_dump();
	}

	return ret;
}

int cthd_engine::user_set_zone_status(std::string name, int status) {
	cthd_zone *zone;

	pthread_mutex_lock(&thd_engine_mutex);
	zone = get_zone(name);
	if (!zone) {
		pthread_mutex_unlock(&thd_engine_mutex);
		return THD_ERROR;
	}

	thd_log_info("Zone Set status %d\n", status);
	if (status)
		zone->set_zone_active();
	else
		zone->set_zone_inactive();

	pthread_mutex_unlock(&thd_engine_mutex);

	return THD_SUCCESS;
}

int cthd_engine::user_get_zone_status(std::string name, int *status) {
	cthd_zone *zone;

	pthread_mutex_lock(&thd_engine_mutex);
	zone = get_zone(name);
	if (!zone) {
		pthread_mutex_unlock(&thd_engine_mutex);
		return THD_ERROR;
	}

	if (zone->zone_active_status())
		*status = 1;
	else
		*status = 0;

	pthread_mutex_unlock(&thd_engine_mutex);

	return THD_SUCCESS;
}

int cthd_engine::user_delete_zone(std::string name) {
	pthread_mutex_lock(&thd_engine_mutex);
	for (unsigned int i = 0; i < zones.size(); ++i) {
		if (zones[i]->get_zone_type() == name) {
			delete zones[i];
			zones.erase(zones.begin() + i);
			break;
		}
	}
	pthread_mutex_unlock(&thd_engine_mutex);

	for (unsigned int i = 0; i < zones.size(); ++i) {
		zones[i]->zone_dump();
	}

	return THD_SUCCESS;
}

int cthd_engine::user_add_cdev(std::string cdev_name, std::string cdev_path,
		int min_state, int max_state, int step) {
	cthd_cdev *cdev;

	pthread_mutex_lock(&thd_engine_mutex);
	// Check if there is existing cdev with this name and path
	cdev = search_cdev(cdev_name);
	if (!cdev) {
		cthd_gen_sysfs_cdev *cdev_sysfs;

		cdev_sysfs = new cthd_gen_sysfs_cdev(current_cdev_index, cdev_path);
		if (!cdev_sysfs) {
			pthread_mutex_unlock(&thd_engine_mutex);
			return THD_ERROR;
		}
		cdev_sysfs->set_cdev_type(cdev_name);
		if (cdev_sysfs->update() != THD_SUCCESS) {
			delete cdev_sysfs;
			pthread_mutex_unlock(&thd_engine_mutex);
			return THD_ERROR;
		}
		cdevs.push_back(cdev_sysfs);
		++current_cdev_index;
		cdev = cdev_sysfs;
	}
	cdev->set_min_state(min_state);
	cdev->set_max_state(max_state);
	cdev->set_inc_dec_value(step);
	pthread_mutex_unlock(&thd_engine_mutex);

	for (unsigned int i = 0; i < cdevs.size(); ++i) {
		cdevs[i]->cdev_dump();
	}

	return THD_SUCCESS;
}

int cthd_engine::parser_init() {
	if (parser_disabled)
		return THD_ERROR;
	if (parser_init_done)
		return THD_SUCCESS;
	if (parser.parser_init(get_config_file()) == THD_SUCCESS) {
		if (parser.start_parse() == THD_SUCCESS) {
			parser.dump_thermal_conf();
			parser_init_done = true;
			return THD_SUCCESS;
		}
	}

	return THD_ERROR;
}

void cthd_engine::parser_deinit() {
	if (parser_init_done) {
		parser.parser_deinit();
		parser_init_done = false;
	}
}
