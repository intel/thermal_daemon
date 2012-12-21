/*
 * thd_engine.h: thermal engine class interface
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

#ifndef THD_ENGINE_H
#define THD_ENGINE_H

#include "thermald.h"
#include "thd_sys_fs.h"
#include "thd_preference.h"
#include "thd_zone.h"
#include "thd_zone_custom.h"
#include "thd_cdev.h"
#include "thd_cdev_custom.h"
#include "thd_nl_wrapper.h"
#include "thd_parse.h"
#include <pthread.h>
#include <poll.h>

#define THD_NUM_OF_POLL_FDS	10
#define MAX_MSG_SIZE 512


typedef enum {
	WAKEUP,
	TERMINATE,
	PREF_CHANGED,
	THERMAL_ZONE_NOTIFY,
	CALIBRATE,
}message_name_t;

typedef struct {
	message_name_t msg_id;
	int			msg_size;
	unsigned long msg[MAX_MSG_SIZE];
}message_capsul_t;

class cthd_engine {

protected:
	std::vector <cthd_zone_custom> zones;
	std::vector <cthd_cdev_custom> cdevs;
	int				cdev_cnt;

private:
	static const int max_thermal_zones = 10;
	static const int max_cool_devs = 50;
	bool status;

	csys_fs thd_sysfs;
	int preference;
	pthread_t thd_engine;
	pthread_attr_t thd_attr;
	pthread_cond_t thd_cond_var;
	pthread_mutex_t thd_cond_mutex;
	struct pollfd poll_fds[THD_NUM_OF_POLL_FDS];
	int write_pipe_fd;
	int wakeup_fd;
	int poll_timeout_msec;
	cthd_nl_wrapper nl_wrapper;

	int proc_message(message_capsul_t *msg);
	void process_pref_change();
	void thermal_zone_change(message_capsul_t *msg);

	void process_terminate();

public:
	cthd_engine();
	virtual ~cthd_engine() {}
	void thd_engine_thread();
	int thd_engine_start();
	int thd_engine_stop();

	bool set_preference(const int pref);
	void thd_engine_terminate();

	cthd_cdev thd_get_cdev_at_index(int index);
	void thd_cdev_dump();
	void send_message(message_name_t msg_id, int size, unsigned char *msg);
	void takeover_thermal_control();
	void giveup_thermal_control();

	virtual int read_thermal_zones();
	virtual int read_cooling_devices();
};

#endif

