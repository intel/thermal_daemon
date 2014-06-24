/*
 * thd_engine.h: thermal engine class interface
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

#ifndef THD_ENGINE_H_
#define THD_ENGINE_H_

#include <pthread.h>
#include <poll.h>
#include <time.h>
#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_sensor.h"
#include "thd_zone.h"
#include "thd_cdev.h"
#include "thd_parse.h"
#include "thd_kobj_uevent.h"
#include "thd_rapl_power_meter.h"

#define MAX_MSG_SIZE 		512
#define THD_NUM_OF_POLL_FDS	10

typedef enum {
	WAKEUP,
	TERMINATE,
	PREF_CHANGED,
	THERMAL_ZONE_NOTIFY,
	CALIBRATE,
	RELOAD_ZONES,
	POLL_ENABLE,
	POLL_DISABLE
} message_name_t;

// This defines whether the thermal control is entirey done by
// this daemon or it just complements, what is done in kernel
typedef enum {
	COMPLEMENTRY, EXCLUSIVE,
} control_mode_t;

typedef struct {
	message_name_t msg_id;
	int msg_size;
	unsigned long msg[MAX_MSG_SIZE];
} message_capsul_t;

typedef struct {
	unsigned int family;
	unsigned int model;
} supported_ids_t;

class cthd_engine {

protected:
	std::vector<cthd_zone *> zones;
	std::vector<cthd_sensor *> sensors;
	std::vector<cthd_cdev *> cdevs;
	int cdev_cnt;
	int zone_count;
	int sensor_count;
	bool parse_thermal_zone_success;
	bool parse_thermal_cdev_success;

private:

	int poll_timeout_msec;
	int wakeup_fd;
	control_mode_t control_mode;
	int write_pipe_fd;
	int preference;
	bool status;
	time_t thz_last_time;
	bool terminate;
	int genuine_intel;
	int has_invariant_tsc;
	int has_aperf;
	bool proc_list_matched;
	int poll_interval_sec;
	cthd_preference thd_pref;
	unsigned int poll_sensor_mask;

	pthread_t thd_engine;
	pthread_attr_t thd_attr;
	pthread_cond_t thd_cond_var;
	pthread_mutex_t thd_cond_mutex;

	std::vector<std::string> zone_preferences;
	static const int thz_notify_debounce_interval = 3;

	struct pollfd poll_fds[THD_NUM_OF_POLL_FDS];

	cthd_kobj_uevent kobj_uevent;

	int proc_message(message_capsul_t *msg);
	void process_pref_change();
	void thermal_zone_change(message_capsul_t *msg);
	void process_terminate();

public:
	static const int max_thermal_zones = 10;
	static const int max_cool_devs = 50;
	static const int def_poll_interval = 4000;
	static const int soft_cdev_start_index = 100;

	cthd_parse parser;
	cthd_rapl_power_meter rapl_power_meter;

	cthd_engine();
	virtual ~cthd_engine();
	void set_control_mode(control_mode_t mode) {
		control_mode = mode;
	}
	void thd_engine_thread();
	int thd_engine_start(bool ignore_cpuid_check);
	int thd_engine_stop();
	int check_cpu_id();

	bool set_preference(const int pref);
	void thd_engine_terminate();
	void thd_engine_calibrate();
	int thd_engine_set_user_max_temp(const char *zone_type,
			const char *user_set_point);
	int thd_engine_set_user_psv_temp(const char *zone_type,
			const char *user_set_point);

	void poll_enable_disable(bool status, message_capsul_t *msg);

	cthd_cdev *thd_get_cdev_at_index(int index);

	void send_message(message_name_t msg_id, int size, unsigned char *msg);

	void takeover_thermal_control();
	void giveup_thermal_control();
	void thd_engine_poll_enable(int sensor_id);
	void thd_engine_poll_disable(int sensor_id);

	void thd_read_default_thermal_sensors();
	void thd_read_default_thermal_zones();
	void thd_read_default_cooling_devices();

	virtual int read_thermal_sensors() {
		return 0;
	}
	;

	virtual int read_thermal_zones() {
		return 0;
	}
	;
	virtual int read_cooling_devices() {
		return 0;
	}
	;

	int use_custom_zones() {
		return parse_thermal_zone_success;
	}
	int use_custom_cdevs() {
		return parse_thermal_cdev_success;
	}

	static const int max_cpu_count = 64;

	time_t last_cpu_update[max_cpu_count];
	virtual bool apply_cpu_operation(int cpu) {
		return false;
	}
	int get_poll_timeout_ms() {
		return poll_timeout_msec;
	}
	int get_poll_timeout_sec() {
		return poll_timeout_msec / 1000;
	}
	void thd_engine_reload_zones();
	bool processor_id_match() {
		return proc_list_matched;
	}
	void set_poll_interval(int val) {
		poll_interval_sec = val;
	}
	int get_preference() {
		return preference;
	}
	cthd_zone *search_zone(std::string name);
	cthd_cdev *search_cdev(std::string name);
	cthd_sensor *search_sensor(std::string name);
	cthd_sensor *get_sensor(int index);
	cthd_zone *get_zone(int index);
	cthd_zone *get_zone(std::string type);

	unsigned int get_zone_count() {
		return zones.size();
	}
	void add_zone(cthd_zone *zone) {
		zones.push_back(zone);
	}

	;
};

#endif /* THD_ENGINE_H_ */
