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

#include <memory>
#include <pthread.h>
#include <poll.h>
#include <time.h>
#include "thd_common.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_sensor.h"
#include "thd_sensor_virtual.h"
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
	RELOAD_ZONES,
	POLL_ENABLE,
	POLL_DISABLE,
	FAST_POLL_ENABLE,
	FAST_POLL_DISABLE,
} message_name_t;

// This defines whether the thermal control is entirely done by
// this daemon or it just complements, what is done in kernel
typedef enum {
	COMPLEMENTRY, EXCLUSIVE,
} control_mode_t;

typedef struct {
	message_name_t msg_id;
	int msg_size;
	unsigned long msg[MAX_MSG_SIZE];
} message_capsul_t;

class cthd_engine {

protected:
	std::vector<std::unique_ptr<cthd_zone>> zones;
	std::vector<std::unique_ptr<cthd_sensor>> sensors;
	std::vector<std::unique_ptr<cthd_cdev>> cdevs;
	int current_cdev_index;
	int current_zone_index;
	int current_sensor_index;
	bool parse_thermal_zone_success;
	bool parse_thermal_cdev_success;
	std::string uuid;
	bool parser_disabled;
	bool adaptive_mode;

private:

	int poll_timeout_msec;
	int wakeup_fd;
	int uevent_fd;
	control_mode_t control_mode;
	int write_pipe_fd;
	int preference;
	bool status;
	time_t thz_last_uevent_time;
	time_t thz_last_temp_ind_time;
	time_t thz_last_update_event_time;
	bool terminate;
	int has_invariant_tsc;
	int has_aperf;
	bool proc_list_matched;
	int poll_interval_sec;
	cthd_preference thd_pref;
	unsigned int poll_sensor_mask;
	unsigned int fast_poll_sensor_mask;
	int saved_poll_interval;
	std::string config_file;

	pthread_t thd_engine;
	pthread_attr_t thd_attr;

	pthread_mutex_t thd_engine_mutex;

	std::vector<std::string> zone_preferences;
	static const int thz_notify_debounce_interval = 3;

	struct pollfd poll_fds[THD_NUM_OF_POLL_FDS];
	int poll_fd_cnt;
	bool rt_kernel;
	cthd_kobj_uevent kobj_uevent;
	bool parser_init_done;

	int proc_message(message_capsul_t *msg);
	void process_pref_change();
	void thermal_zone_change(message_capsul_t *msg);
	void process_terminate();
	void check_for_rt_kernel();

public:
	static const int max_thermal_zones = 10;
	static const int max_cool_devs = 50;
	static const int def_poll_interval = 4000;
	static const int soft_cdev_start_index = 100;

	cthd_parse parser;
	cthd_rapl_power_meter rapl_power_meter;

	cthd_engine(std::string _uuid);
	virtual ~cthd_engine();
	void set_control_mode(control_mode_t mode) {
		control_mode = mode;
	}
	control_mode_t get_control_mode() {
		return control_mode;
	}
	void thd_engine_thread();
	virtual int thd_engine_init(bool ignore_cpuid_check, bool adaptive = false);
	virtual int thd_engine_start();
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
	void fast_poll_enable_disable(bool status, message_capsul_t *msg);

	cthd_cdev *thd_get_cdev_at_index(int index);

	void send_message(message_name_t msg_id, int size, unsigned char *msg);

	void takeover_thermal_control();
	void giveup_thermal_control();
	void thd_engine_poll_enable(int sensor_id);
	void thd_engine_poll_disable(int sensor_id);

	void thd_engine_fast_poll_enable(int sensor_id);
	void thd_engine_fast_poll_disable(int sensor_id);

	void thd_read_default_thermal_sensors();
	void thd_read_default_thermal_zones();
	void thd_read_default_cooling_devices();

	virtual void update_engine_state() {};
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
	int get_poll_interval() {
		return poll_interval_sec;
	}
	void set_poll_interval(int val) {
		poll_interval_sec = val;
	}
	int get_preference() {
		return preference;
	}
	void set_config_file(std::string conf_file) {
		config_file = std::move(conf_file);
	}
	std::string get_config_file() {
		return config_file;
	}
	virtual ppcc_t *get_ppcc_param(const std::string& name);
	virtual int search_idsp(std::string name) {
		return THD_ERROR;
	}
	cthd_zone *search_zone(const std::string& name);
	cthd_cdev *search_cdev(const std::string& name);
	cthd_cdev *match_cdev(const std::string& name);
	cthd_sensor *search_sensor(const std::string& name);
	cthd_sensor *get_sensor(int index);
	cthd_zone *get_zone(int index);
	cthd_zone *get_zone(const std::string& type);
	int get_sensor_temperature(int index, unsigned int *temperature);

	unsigned int get_sensor_count() {
		return sensors.size();
	}

	unsigned int get_zone_count() {
		return zones.size();
	}

	unsigned int get_cdev_count() {
		return cdevs.size();
	}

	void add_zone(std::unique_ptr<cthd_zone> zone) {
		zones.push_back(std::move(zone));
	}

	bool rt_kernel_status() {
		return rt_kernel;
	}

	virtual void workarounds() {
	}

	void thd_engine_lock() {
		pthread_mutex_lock(&thd_engine_mutex);
	}

	void thd_engine_unlock() {
		pthread_mutex_unlock(&thd_engine_mutex);
	}

	// User/External messages
	int user_add_sensor(std::string name, std::string path);
	cthd_sensor *user_get_sensor(unsigned int index);
	cthd_zone *user_get_zone(unsigned int index);
	int user_add_virtual_sensor(std::string name, std::string dep_sensor,
			double slope, double intercept);

	int user_set_psv_temp(const std::string& name, unsigned int temp);
	int user_set_max_temp(const std::string& name, unsigned int temp);
	int user_add_zone(std::string zone_name, unsigned int trip_temp,
			std::string sensor_name, std::string cdev_name);
	int user_set_zone_status(const std::string& name, int status);
	int user_get_zone_status(const std::string& name, int *status);
	int user_delete_zone(const std::string& name);

	int user_add_cdev(std::string cdev_name, std::string cdev_path,
			int min_state, int max_state, int step);
	cthd_cdev *user_get_cdev(unsigned int index);

	void enable_power_floor_event();
	int parser_init();
	void parser_deinit();
	int debug_mode_on(void);
};

#endif /* THD_ENGINE_H_ */
