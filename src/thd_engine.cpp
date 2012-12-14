/*
 * thd_engine.cpp: thermal engine class implementation
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

#include "thd_engine.h"

static void *cthd_engine_thread(void *arg);

cthd_engine::cthd_engine()
	: thd_sysfs("/sys/class/thermal/")
{
}

void cthd_engine::thd_engine_thread()
{
	int n, i;
	int result;

	thd_log_debug("thd_engine_thread begin\n");
	for (;;) {

		n = poll(poll_fds, THD_NUM_OF_POLL_FDS, poll_timeout_msec);
		thd_log_debug("poll exit %d \n", n);
		if (n<0) {
			thd_log_warn("Write to pipe failed \n");
			continue;
		}
		if (n == 0) {
			// Polling mode enabled. Trigger a temp change message
			for (i=0; i<zones.size(); ++i)	{
				cthd_zone zone = zones[i];
				zone.zone_temperature_notification(0, 0);
			}
		}
		if (poll_fds[0].revents & POLLIN) {
			// Netlink message
			thd_log_debug("Netlink message\n");
			nl_wrapper.genl_message_indication();
		}
		if (poll_fds[wakeup_fd].revents & POLLIN) {
			message_capsul_t msg;

			thd_log_debug("wakeup fd event\n");
			int result = read(poll_fds[wakeup_fd].fd, &msg, sizeof(message_capsul_t));
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


bool cthd_engine::set_preference(const int pref)
{
	return TRUE;
}

int cthd_engine::thd_engine_start()
{
	int i, ret;
    	int wake_fds[2];

	ret = pipe(wake_fds);
	if (ret) {
		thd_log_error("Thermal sysfs: pipe creation failed %d: ", ret);
		return THD_FATAL_ERROR;
	}
	fcntl(wake_fds[0], F_SETFL, O_NONBLOCK);
	fcntl(wake_fds[1], F_SETFL, O_NONBLOCK);
	write_pipe_fd = wake_fds[1];
	wakeup_fd = THD_NUM_OF_POLL_FDS - 1;

	memset(poll_fds, 0, sizeof(poll_fds));
	poll_fds[wakeup_fd].fd = wake_fds[0];
	poll_fds[wakeup_fd].events = POLLIN;
	poll_fds[wakeup_fd].revents = 0;
	poll_timeout_msec = -1;
	if (thd_poll_interval) {
		thd_log_warn("Polling mode is enabled: %d\n", thd_poll_interval);
		poll_timeout_msec = thd_poll_interval * 1000;
	}
	// Check existance of sysfs of thermal
	if (!thd_sysfs.exists()) {
		thd_log_error("Thermal sysfs is not present: ");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	ret = read_thermal_zones();
	if (ret != THD_SUCCESS) {
		thd_log_error("Thermal sysfs Error in reading thermal zones");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	ret = read_cooling_devices();
	if (ret != THD_SUCCESS) {
		thd_log_error("Thermal sysfs Error in reading cooling devs");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	// condition variable
	pthread_cond_init (&thd_cond_var, NULL);
	pthread_mutex_init(&thd_cond_mutex, NULL);
	// Create thread
	pthread_attr_init(&thd_attr);
	pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&thd_engine, &thd_attr, cthd_engine_thread, (void *)this);

	// Netlink stuff
	nl_wrapper.genl_acpi_family_open(this);
	if (nl_wrapper.rtnl_fd() < 0) {
		thd_log_warn("Invalid rtnl handle\n");
		return -1;
	}
	poll_fds[0].fd = nl_wrapper.rtnl_fd();
	poll_fds[0].events = POLLIN;
	poll_fds[0].revents = 0;
	return ret;
}

int cthd_engine::thd_engine_stop()
{
	return THD_SUCCESS;
}

int cthd_engine::read_thermal_zones()
{
	int count = 0;

	// Check for the presence of thermal zones
	if (thd_sysfs.exists("thermal_zone")) {
		cthd_zone_custom zone(count);
		zones.push_back(zone);
		++count;
	}
	for (int i=0; i<max_thermal_zones; ++i) {
		std::stringstream tzone;
		tzone << "thermal_zone" << i;
		if (thd_sysfs.exists(tzone.str().c_str())) {
			cthd_zone_custom zone(count);
			if (zone.thd_update_zones() != THD_SUCCESS)
				continue;
			zones.push_back(zone);
			++count;
		}		
	}
	thd_log_debug("%d thermal zones present\n", count);
	if (!count) {
		thd_log_error("Thermal sysfs: No Zones present: ");
		return THD_FATAL_ERROR;
	}

	return THD_SUCCESS;
}

int cthd_engine::read_cooling_devices()
{
	cdev_cnt = 0;
	for (int i=0; i<max_cool_devs; ++i) {
		std::stringstream tcdev;
		tcdev << "cooling_device" << i;
		if (thd_sysfs.exists(tcdev.str().c_str())) {
			cthd_cdev_custom cdev(cdev_cnt);
			if (cdev.update() != THD_SUCCESS)
				continue;
			cdevs.push_back(cdev);
			++cdev_cnt;
		}
	}
	thd_log_debug("%d cooling device present\n", cdev_cnt);
	if (!cdev_cnt) {
		thd_log_error("Thermal sysfs: No cooling devices: ");
		return THD_FATAL_ERROR;
	}

	return THD_SUCCESS;
}

static void *cthd_engine_thread(void *arg)
{
	cthd_engine *obj = (cthd_engine *)arg;

	obj->thd_engine_thread();
}

void cthd_engine::send_message(message_name_t msg_id, int size, unsigned char *msg)
{
	message_capsul_t msg_cap;

	msg_cap.msg_id = msg_id;
	msg_cap.msg_size = (size > MAX_MSG_SIZE) ? MAX_MSG_SIZE :size;
	if (msg)
		memcpy(msg_cap.msg, msg, msg_cap.msg_size);
	int result = write(write_pipe_fd, &msg_cap, sizeof(message_capsul_t));
	if (result < 0)
		thd_log_warn("Write to pipe failed \n");
}

void cthd_engine::process_pref_change()
{
	int new_pref;
	cthd_preference thd_pref;

	new_pref = thd_pref.get_preference();
	if (preference != new_pref) {
		thd_log_warn("Preference changed \n");
	}
}

void cthd_engine::thermal_zone_change(message_capsul_t *msg)
{
	int i;

	thermal_zone_notify_t *pmsg = (thermal_zone_notify_t*)msg->msg;
	for (i=0; i<zones.size(); ++i)	{
		cthd_zone zone = zones[i];
		zone.zone_temperature_notification(pmsg->type, pmsg->data);
	}
}

int cthd_engine::proc_message(message_capsul_t *msg)
{
	int ret = 0;

	thd_log_debug("Receieved message %d\n", msg->msg_id);
	switch (msg->msg_id) {
		case WAKEUP:
			break;
		case TERMINATE:
			ret = -1;
	 		break;
		case PREF_CHANGED:
			process_pref_change();
			break;
		case THERMAL_ZONE_NOTIFY:
			thermal_zone_change(msg);
			break;
		default:
			break;
	}

	return ret;
}

void cthd_engine::thd_cdev_dump()
{
	int i;

	for (i=0; i<cdev_cnt; ++i){
		cthd_cdev cdev = cdevs[i];
		thd_log_debug("cdev index %d\n", cdev.thd_cdev_get_index());

	}
}

