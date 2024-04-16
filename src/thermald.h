/*
 * thermald.h: Thermal Daemon common header file
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
#ifndef THD_THERMALD_H
#define THD_THERMALD_H

#include <stdio.h>
#include <getopt.h>
#include <locale.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>

#ifdef ANDROID

#define LOG_NDEBUG 1
#undef LOG_TAG
#define LOG_TAG "THERMALD"
#include <utils/Log.h>
#include <log/log.h>
#include <cutils/properties.h>

#define thd_log_fatal	ALOGE
#define thd_log_error	ALOGE
#define thd_log_warn	ALOGW
#define thd_log_msg	ALOGI
#if LOG_DEBUG_INFO == 1
#define thd_log_info	ALOGI
#define thd_log_debug 	ALOGD
#else
#define thd_log_info(...)
#define thd_log_debug(...)
#endif

#else

#include "config.h"

// Keeping the logging flag enabled for non-android cases
#define LOG_DEBUG_INFO	1

#define LOCKF_SUPPORT
#ifdef GLIB_SUPPORT
#include <glib.h>
#include <glib/gi18n.h>
#include <gmodule.h>

// Log macros
#define thd_log_fatal		g_error		// Print error and terminate
#define thd_log_error		g_critical
#define thd_log_warn		g_warning
#define thd_log_msg		g_message
#define thd_log_debug		g_debug
#define thd_log_info(...)	g_log(NULL, G_LOG_LEVEL_INFO, __VA_ARGS__)
#else
static int dummy_printf(const char *__restrict __format, ...) {
	return 0;
}
#define thd_log_fatal		printf
#define thd_log_error		printf
#define thd_log_warn		printf
#define thd_log_msg		printf
#define thd_log_debug		dummy_printf
#define thd_log_info		printf
#endif
#endif
// Common return value defines
#define THD_SUCCESS			0
#define THD_ERROR			-1
#define THD_FATAL_ERROR		-2

// Dbus related
/* Well-known name for this service. */
#define THD_SERVICE_NAME        	"org.freedesktop.thermald"
#define THD_SERVICE_OBJECT_PATH 	"/org/freedesktop/thermald"
#define THD_SERVICE_INTERFACE		"org.freedesktop.thermald"

class cthd_engine;
class cthd_engine_therm_sysfs;
extern cthd_engine *thd_engine;
extern int thd_poll_interval;
extern bool thd_ignore_default_control;
extern bool workaround_enabled;
extern bool disable_active_power;
extern bool ignore_critical;
#endif
