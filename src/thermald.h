/*
 * thermald.h: Thermal Daemon common header file
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
#ifndef THD_THERMALD_H
#define THD_THERMALD_H

#include <config.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
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
#include <glib/gi18n.h>
#include <gmodule.h>
#include <string.h>

// Log macros
#define thd_log_fatal		g_error		// Print error and terminate
#define thd_log_error		g_critical
#define thd_log_warn		g_warning
#define thd_log_debug		g_debug
#define thd_log_info(...)	g_log(NULL, G_LOG_LEVEL_INFO, __VA_ARGS__)

// Common return value defines
#define THD_SUCCESS			0
#define THD_ERROR			-1
#define THD_FATAL_ERROR		-2

// Dbus related
/* Well-known name for this service. */
#define THD_SERVICE_NAME        	"org.thermald.control"
#define THD_SERVICE_OBJECT_PATH 	"/org/thermald/settings"
#define THD_SERVICE_INTERFACE		"org.thermald.value"

class cthd_engine;
class cthd_engine_therm_sysfs;
extern cthd_engine *thd_engine;
extern int thd_poll_interval;

#endif

