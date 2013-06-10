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

#include "thd_engine.h"
#include "thd_topology.h"
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <locale>

static void *cthd_engine_thread(void *arg);
static void *cthd_calibration_engine_thread(void *arg);

cthd_engine::cthd_engine(): poll_timeout_msec( - 1), wakeup_fd(0), control_mode
	(COMPLEMENTRY), write_pipe_fd(0), cdev_cnt(0), preference(0), status(true),
	parse_thermal_zone_success(false),
	parse_thermal_cdev_success(false), zone_count(0), thz_last_time(0), terminate(false){}

cthd_engine::~cthd_engine()
{
	int i;

	for(i = 0; i < zones.size(); ++i)
	{
		delete zones[i];
	}
	zones.erase(zones.begin(), zones.begin() + i - 1);
	for(i = 0; i < cdevs.size(); ++i)
	{
		delete cdevs[i];
	}
	cdevs.erase(cdevs.begin(), cdevs.begin() + i - 1);
}

void cthd_engine::thd_engine_thread()
{
	int n, i;
	int result;

	thd_log_info("thd_engine_thread begin\n");
	for(;;)
	{
		if (terminate)
			break;
		n = poll(poll_fds, THD_NUM_OF_POLL_FDS, poll_timeout_msec);
		thd_log_debug("poll exit %d \n", n);
		if(n < 0)
		{
			thd_log_warn("Write to pipe failed \n");
			continue;
		}
		if(n == 0)
		{
			if(!status)
			{
				thd_log_warn("Thermal Daemon is disabled \n");
				continue;
			}
			// Polling mode enabled. Trigger a temp change message
			for(i = 0; i < zones.size(); ++i)
			{
				cthd_zone *zone = zones[i];
				zone->zone_temperature_notification(0, 0);
			}
		}
		if(poll_fds[0].revents & POLLIN)
		{
			// Kobj uevent
			if (kobj_uevent.check_for_event())
			{
				time_t tm;

				time(&tm);
				thd_log_debug("kobj uevent for thermal\n");
				if ((tm - thz_last_time) >= thz_notify_debounce_interval)
				{
					for(i = 0; i < zones.size(); ++i)
					{
						cthd_zone *zone = zones[i];
						zone->zone_temperature_notification(0, 0);
					}
				}
				else
				{
					thd_log_debug("IGNORE THZ kevent\n");
				}
				thz_last_time = tm;
			}
		}
		if(poll_fds[wakeup_fd].revents & POLLIN)
		{
			message_capsul_t msg;

			thd_log_debug("wakeup fd event\n");
			int result = read(poll_fds[wakeup_fd].fd, &msg, sizeof(message_capsul_t));
			if(result < 0)
			{
				thd_log_warn("read on wakeup fd failed\n");
				poll_fds[wakeup_fd].revents = 0;
				continue;
			}
			if(proc_message(&msg) < 0)
			{
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

	check_cpu_id();

	// Pipe is used for communication between two processes
	ret = pipe(wake_fds);
	if(ret)
	{
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

	poll_timeout_msec =  - 1;
	if(thd_poll_interval)
	{
		thd_log_warn("Polling mode is enabled: %d\n", thd_poll_interval);
		poll_timeout_msec = thd_poll_interval * 1000;
	}

	ret = read_cooling_devices();
	if(ret != THD_SUCCESS)
	{
		thd_log_error("Thermal sysfs Error in reading cooling devs");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	ret = read_thermal_zones();
	if(ret != THD_SUCCESS)
	{
		thd_log_error("No thermal sensors found");
		// This is a fatal error and daemon will exit
		return THD_FATAL_ERROR;
	}

	poll_fds[0].fd = kobj_uevent.kobj_uevent_open();
	if(poll_fds[0].fd < 0)
	{
		thd_log_warn("Invalid kobj_uevent handle\n");
		goto skip_kobj;
	}
	thd_log_info("FD = %d\n", poll_fds[0].fd);
	kobj_uevent.register_dev_path((char *)"/devices/virtual/thermal/thermal_zone");
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
	ret = pthread_create(&thd_engine, &thd_attr, cthd_engine_thread, (void*)this);
#else 
	{
		pid_t childpid;

		if((childpid = fork()) ==  - 1)
		{
			perror("fork");
			exit(1);
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
#ifdef PER_CPU_P_STATE_CONTROL
	// calibration thread
	pthread_attr_init(&thd_attr);
	pthread_attr_setdetachstate(&thd_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&cal_thd_engine, &thd_attr,
	cthd_calibration_engine_thread, (void*)this);
#endif
	cthd_preference thd_pref;
	preference = thd_pref.get_preference();
	thd_log_info("Current user preference is %d\n", preference);

	if(control_mode == EXCLUSIVE)
	{
		thd_log_info("Control is taken over from kernel\n");
		takeover_thermal_control();
	}
	return ret;
}

int cthd_engine::thd_engine_stop()
{
	return THD_SUCCESS;
}

static void *cthd_engine_thread(void *arg)
{
	cthd_engine *obj = (cthd_engine*)arg;

	obj->thd_engine_thread();

	return NULL;
}

#ifdef PER_CPU_P_STATE_CONTROL
static void *cthd_calibration_engine_thread(void *arg)
{
	int ret;
	cthd_topology topo;
	cthd_engine *obj = (cthd_engine*)arg;

	if(topo.check_config_saved())
	{
		thd_log_warn("Already calibrated\n");
		return NULL;
	}
	for(;;)
	{
		sleep(60);
		ret = topo.calibrate();
		if(ret == 0)
		{
			obj->send_message(RELOAD_ZONES, 0, NULL);
			break; // process done
		}
	}
	thd_log_warn("Calibration thread exit\n");
	return NULL;
}
#endif

void cthd_engine::send_message(message_name_t msg_id, int size, unsigned char
	*msg)
{
	message_capsul_t msg_cap;

	memset(&msg_cap, 0, sizeof(message_capsul_t));

	msg_cap.msg_id = msg_id;
	msg_cap.msg_size = (size > MAX_MSG_SIZE) ? MAX_MSG_SIZE : size;
	if(msg)
		memcpy(msg_cap.msg, msg, msg_cap.msg_size);
	int result = write(write_pipe_fd, &msg_cap, sizeof(message_capsul_t));
	if(result < 0)
		thd_log_warn("Write to pipe failed \n");
}

void cthd_engine::process_pref_change()
{
	int new_pref;
	cthd_preference thd_pref;

	new_pref = thd_pref.get_preference();
	if(new_pref == PREF_DISABLED)
	{
		status = false;
		return ;
	}
	status = true;
	if(preference != new_pref)
	{
		thd_log_warn("Preference changed \n");
	}

	for(int i = 0; i < zones.size(); ++i)
	{
		cthd_zone *zone = zones[i];
		zone->update_zone_preference();
	}

	if(preference != 0)
		takeover_thermal_control();
}

void cthd_engine::thd_engine_terminate()
{
	send_message(TERMINATE, 0, NULL);
	sleep(1);
	process_terminate();
}

void cthd_engine::thd_engine_calibrate()
{
#ifdef PER_CPU_P_STATE_CONTROL
	send_message(CALIBRATE, 0, NULL);
#endif
}

int cthd_engine::thd_engine_set_user_set_point(const char *user_set_point)
{
	std::string str(user_set_point);

	thd_log_debug("thd_engine_set_user_set_point %s\n", user_set_point);

	std::locale loc;
	if (std::isdigit(str[0],loc) == 0)
	{
		thd_log_warn("thd_engine_set_user_set_point Invalid set point\n");
		return THD_ERROR;
	}
	std::stringstream filename;
	filename << TDCONFDIR << "/" << "thd_user_set_point.conf";

	std::ofstream fout(filename.str().c_str());
	if(!fout.good())
	{
		return THD_ERROR;
	}
	fout << str;
	fout.close();

	return THD_SUCCESS;
}

void cthd_engine::thermal_zone_change(message_capsul_t *msg)
{
	int i;

	thermal_zone_notify_t *pmsg = (thermal_zone_notify_t*)msg->msg;
	for(i = 0; i < zones.size(); ++i)
	{
		cthd_zone *zone = zones[i];
		if(zone->zone_active_status())
			zone->zone_temperature_notification(pmsg->type, pmsg->data);
		else
		{
			thd_log_debug("zone is not active\n");
		}
	}
}

int cthd_engine::proc_message(message_capsul_t *msg)
{
	int ret = 0;

	thd_log_debug("Receieved message %d\n", msg->msg_id);
	switch(msg->msg_id)
	{
		case WAKEUP:
			break;
		case TERMINATE:
			thd_log_warn("Terminating ...\n");

			ret =  - 1;
			terminate = true;
			break;
		case PREF_CHANGED:
			process_pref_change();
			break;
		case THERMAL_ZONE_NOTIFY:
			if(!status)
			{
				thd_log_warn("Thermal Daemon is disabled \n");
				break;
			}
			thermal_zone_change(msg);
			break;
#ifdef PER_CPU_P_STATE_CONTROL
		case CALIBRATE:
			{
				cthd_topology topo;
				topo.calibrate();
			}
			break;
#endif
		case RELOAD_ZONES:
			thd_engine_reload_zones();
			break;
		case POLL_ENABLE:
			if (!thd_poll_interval)
				poll_timeout_msec = def_poll_interval;
			break;
		case POLL_DISABLE:
			if (!thd_poll_interval)
				poll_timeout_msec = -1;
			break;
		default:
			break;
	}

	return ret;
}

cthd_cdev *cthd_engine::thd_get_cdev_at_index(int index)
{
	for(int i = 0; i < cdev_cnt; ++i)
	{
		if(cdevs[i]->thd_cdev_get_index() == index)
			return cdevs[i];
	}
	return NULL;
}

void cthd_engine::takeover_thermal_control()
{
	csys_fs sysfs("/sys/class/thermal/");

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";

	thd_log_info("Taking over thermal control \n");
	if ((dir = opendir(base_path.c_str())) != NULL)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (!strncmp(entry->d_name, "thermal_zone", strlen("thermal_zone")))
			{
				int i;

				i = atoi(entry->d_name+strlen("thermal_zone"));
				std::stringstream policy;
				std::stringstream type;
				std::string type_val;
				std::string curr_policy;

				type << "thermal_zone" << i << "/type";
				if(sysfs.exists(type.str().c_str()))
				{
					sysfs.read(type.str(), type_val);
				}
				if (type_val != "acpitz")
					continue;
				policy << "thermal_zone" << i << "/policy";
				if(sysfs.exists(policy.str().c_str()))
				{
					sysfs.read(policy.str(), curr_policy);
					zone_preferences.push_back(curr_policy);
					sysfs.write(policy.str(), "user_space");
				}
			}
		}
		closedir(dir);
	}
}

void cthd_engine::giveup_thermal_control()
{
	if(control_mode != EXCLUSIVE)
		return;

	if (zone_preferences.size() == 0)
		return;

	thd_log_info("Giving up thermal control \n");

	csys_fs sysfs("/sys/class/thermal/");

	DIR *dir;
	struct dirent *entry;
	const std::string base_path = "/sys/class/thermal/";
	int cnt = 0;
	if ((dir = opendir(base_path.c_str())) != NULL)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (!strncmp(entry->d_name, "thermal_zone", strlen("thermal_zone")))
			{
				int i;

				i = atoi(entry->d_name+strlen("thermal_zone"));
				std::stringstream policy;
				std::stringstream type;
				std::string type_val;

				type << "thermal_zone" << i << "/type";
				if(sysfs.exists(type.str().c_str()))
				{
					sysfs.read(type.str(), type_val);
				}
				if (type_val != "acpitz")
					continue;
				policy << "thermal_zone" << i << "/policy";
				if(sysfs.exists(policy.str().c_str()))
				{
					sysfs.write(policy.str(), zone_preferences[cnt++]);
				}
			}
		}
		closedir(dir);
	}
}

void cthd_engine::process_terminate()
{
	thd_log_warn("termiating on user request ..\n");
	giveup_thermal_control();
}

void cthd_engine::thd_engine_poll_enable()
{
	send_message(POLL_ENABLE, 0, NULL);
}

void cthd_engine::thd_engine_poll_disable()
{
	send_message(POLL_DISABLE, 0, NULL);
}

void cthd_engine::thd_engine_reload_zones()
{
	thd_log_warn(" Reloading zones\n");
	for(int i = 0; i < zones.size(); ++i)
	{
		cthd_zone *zone = zones[i];
		delete zone;
	}
	zones.clear();

	int ret = read_thermal_zones();
	if(ret != THD_SUCCESS)
	{
		thd_log_error("No thermal sensors found");
		// This is a fatal error and daemon will exit
		return;
	}
}

// Add any tested platform ids in this table
static supported_ids_t id_table[] = {
		{6, 0x2a}, // Sandybridge
		{6, 0x2d}, // Sandybridge
		{6, 0x3a}, // IvyBridge
		{6, 0x45}, // Haswell ULT */

		{0, 0} // Last Invalid entry
};

int cthd_engine::check_cpu_id()
{
	// Copied from turbostat program
	unsigned int eax, ebx, ecx, edx, max_level;
	unsigned int fms, family, model, stepping;
	genuine_intel = 0;
	int i;
	bool valid = false;

	proc_list_matched = false;
	eax = ebx = ecx = edx = 0;

	asm("cpuid": "=a"(max_level), "=b"(ebx), "=c"(ecx), "=d"(edx): "a"(0));

	if(ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
		genuine_intel = 1;
	if(genuine_intel == 0)
	{
		thd_log_warn("Not running on a genuine Intel CPU!\n");
	}
	asm("cpuid": "=a"(fms), "=c"(ecx), "=d"(edx): "a"(1): "ebx");
	family = (fms >> 8) &0xf;
	model = (fms >> 4) &0xf;
	stepping = fms &0xf;
	if(family == 6 || family == 0xf)
		model += ((fms >> 16) &0xf) << 4;

	thd_log_warn("%d CPUID levels; family:model:stepping 0x%x:%x:%x (%d:%d:%d)\n",
	max_level, family, model, stepping, family, model, stepping);

	while(id_table[i].family)
	{
		if (id_table[i].family == family && id_table[i].model == model)
		{
			proc_list_matched = true;
			valid = true;
			break;
		}
		i++;
	}
	if (!valid)
	{
		thd_log_warn(" No support RAPL and Intel P state driver\n");
	}

	if(!(edx &(1 << 5)))
	{
		thd_log_warn("No MSR supported on processor \n");
	}

	return THD_SUCCESS;
}

