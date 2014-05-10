/*
 * main.cpp: Thermal Daemon entry point
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
 * This is the main entry point for thermal daemon. This has main function
 * which parses command line arguments, set up dbus server and log related
 * functions.
 */

/* This implements main() function. This will parse command line options and
 * a new instance of cthd_engine object. By default it will create a engine
 * which uses dts engine, which DTS sensor and use P states to control
 * temperature, without any configuration. Alternatively if the
 * thermal-conf.xml has exact UUID match then it can use the zones and
 * cooling devices defined it to control thermals. This file will allow fine
 * tune ACPI thermal config or create new thermal config using custom
 * sensors.
 * Dbus interface allows user to switch between active/passive thermal controls
 * if the thermal-conf.xml defines parameters.
 */

#include <syslog.h>
#include <signal.h>
#include "thermald.h"
#include "thd_preference.h"
#include "thd_engine.h"
#include "thd_engine_default.h"
#include "thd_parse.h"
#include <syslog.h>

#if !defined(TD_DIST_VERSION)
#define TD_DIST_VERSION PACKAGE_VERSION
#endif

typedef struct {
	GObject parent;
} PrefObject;

typedef struct {
	GObjectClass parent;
} PrefObjectClass;

GType pref_object_get_type(void);
#define MAX_DBUS_REPLY_STR_LEN	20
#define PREF_TYPE_OBJECT (pref_object_get_type())
G_DEFINE_TYPE(PrefObject, pref_object, G_TYPE_OBJECT)

gboolean thd_dbus_interface_calibrate(PrefObject *obj, GError **error);
gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error);
gboolean thd_dbus_interface_set_current_preference(PrefObject *obj, gchar *pref,
		GError **error);
gboolean thd_dbus_interface_get_current_preference(PrefObject *obj,
		gchar **pref_out, GError **error);
gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error);
gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error);
// This is a generated file, which expects the above prototypes
#include "thd-dbus-interface.h"

// Default log level
static int thd_log_level = G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL
		| G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;

// Daemonize or not
static gboolean thd_daemonize;

// Disable dbus
static gboolean dbus_enable;

// poll mode
int thd_poll_interval = 4; //in seconds

// check cpuid
static gboolean ignore_cpuid_check = false;

// Thermal engine
cthd_engine *thd_engine;

gboolean exclusive_control = FALSE;

// DBUS Related functions

// Dbus object initialization
static void pref_object_init(PrefObject *obj) {
	g_assert(obj != NULL);
}

// Dbus object class initialization
static void pref_object_class_init(PrefObjectClass *_class) {
	g_assert(_class != NULL);

	dbus_g_object_type_install_info(PREF_TYPE_OBJECT,
			&dbus_glib_thd_dbus_interface_object_info);
}

// Callback function called to inform a sent value via dbus
gboolean thd_dbus_interface_set_current_preference(PrefObject *obj, gchar *pref,
		GError **error) {
	int ret;

	thd_log_debug("thd_dbus_interface_set_current_preference %s\n",
			(char*) pref);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	ret = thd_pref.set_preference((char*) pref);
	thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return ret;
}

// Callback function called to get value via dbus
gboolean thd_dbus_interface_get_current_preference(PrefObject *obj,
		gchar **pref_out, GError **error) {
	thd_log_debug("thd_dbus_interface_get_current_preference\n");
	g_assert(obj != NULL);
	gchar *value_out;
	static char *pref_str;

	pref_str = g_new(char, MAX_DBUS_REPLY_STR_LEN);

	if (!pref_str)
		return FALSE;

	cthd_preference thd_pref;
	value_out = (gchar*) thd_pref.get_preference_cstr();
	if (!value_out) {
		g_free(pref_str);
		return FALSE;
	}
	strncpy(pref_str, value_out, MAX_DBUS_REPLY_STR_LEN);
	free(value_out);
	thd_log_debug("thd_dbus_interface_get_current_preference out :%s\n",
			pref_str);
	*pref_out = pref_str;

	return TRUE;
}

gboolean thd_dbus_interface_calibrate(PrefObject *obj, GError **error) {
//	thd_engine->thd_engine_calibrate();
	return TRUE;
}

gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error) {
	thd_engine->thd_engine_terminate();
	return TRUE;
}

gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error) {
	thd_log_debug("thd_dbus_interface_set_user_set_point %s:%s\n", zone_name,
			temperature);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	if (thd_engine->thd_engine_set_user_max_temp(zone_name,
			(char*) temperature) == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return TRUE;
}

gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error) {
	thd_log_debug("thd_dbus_interface_set_user_passive_temperature %s:%s\n", zone_name,
			temperature);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	if (thd_engine->thd_engine_set_user_psv_temp(zone_name,
			(char*) temperature) == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return TRUE;
}

// g_log handler. All logs will be directed here
void thd_logger(const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, gpointer user_data) {
	if (!(thd_log_level & log_level))
		return;
	if (thd_daemonize) {
		int syslog_priority;
		switch (log_level) {
		case G_LOG_LEVEL_ERROR:
			syslog_priority = LOG_CRIT;
			break;
		case G_LOG_LEVEL_CRITICAL:
			syslog_priority = LOG_ERR;
			break;
		case G_LOG_LEVEL_WARNING:
			syslog_priority = LOG_WARNING;
			break;
		case G_LOG_LEVEL_MESSAGE:
			syslog_priority = LOG_NOTICE;
			break;
		case G_LOG_LEVEL_DEBUG:
			syslog_priority = LOG_DEBUG;
			break;
		case G_LOG_LEVEL_INFO:
		default:
			syslog_priority = LOG_INFO;
			break;
		}
		syslog(syslog_priority, "%s", message);
	} else
		g_print("%s", message);
}

static GMainLoop *g_main_loop;

// Setup dbus server
static int thd_dbus_server_proc(gboolean no_daemon) {
	DBusGConnection *bus;
	DBusGProxy *bus_proxy;
	GMainLoop *main_loop;
	GError *error = NULL;
	guint result;
	PrefObject *value_obj;

	thd_engine = NULL;
	// Initialize the GType/GObject system
	g_type_init();

	// Create a main loop that will dispatch callbacks
	g_main_loop = main_loop = g_main_loop_new(NULL, FALSE);
	if (main_loop == NULL) {
		thd_log_error("Couldn't create GMainLoop:\n");
		return THD_FATAL_ERROR;
	}
	if (dbus_enable) {
		bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
		if (error != NULL) {
			thd_log_error("Couldn't connect to session bus: %s:\n",
					error->message);
			return THD_FATAL_ERROR;
		}

		// Get a bus proxy instance
		bus_proxy = dbus_g_proxy_new_for_name(bus, DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
		if (bus_proxy == NULL) {
			thd_log_error("Failed to get a proxy for D-Bus:\n");
			return THD_FATAL_ERROR;
		}

		thd_log_debug("Registering the well-known name (%s)\n",
				THD_SERVICE_NAME);
		// register the well-known name
		if (!dbus_g_proxy_call(bus_proxy, "RequestName", &error, G_TYPE_STRING,
				THD_SERVICE_NAME, G_TYPE_UINT, 0, G_TYPE_INVALID, G_TYPE_UINT,
				&result, G_TYPE_INVALID)) {
			thd_log_error("D-Bus.RequestName RPC failed: %s\n", error->message);
			return THD_FATAL_ERROR;
		}
		thd_log_debug("RequestName returned %d.\n", result);
		if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
			thd_log_error("Failed to get the primary well-known name:\n");
			return THD_FATAL_ERROR;
		}
		value_obj = (PrefObject*) g_object_new(PREF_TYPE_OBJECT, NULL);
		if (value_obj == NULL) {
			thd_log_error("Failed to create one Value instance:\n");
			return THD_FATAL_ERROR;
		}

		thd_log_debug("Registering it on the D-Bus.\n");
		dbus_g_connection_register_g_object(bus, THD_SERVICE_OBJECT_PATH,
				G_OBJECT(value_obj));
	}
	if (!no_daemon) {
		printf("Ready to serve requests: Daemonizing.. %d\n", thd_daemonize);
		thd_log_info(
				"thermald ver %s: Ready to serve requests: Daemonizing..\n",
				TD_DIST_VERSION);

		if (daemon(0, 0) != 0) {
			thd_log_error("Failed to daemonize.\n");
			return THD_FATAL_ERROR;
		}
	}

	thd_engine = new cthd_engine_default();
	if (exclusive_control)
		thd_engine->set_control_mode(EXCLUSIVE);

	// Initialize thermald objects
	thd_engine->set_poll_interval(thd_poll_interval);
	if (thd_engine->thd_engine_start(ignore_cpuid_check) != THD_SUCCESS) {
		thd_log_error("THD engine start failed:\n");
		closelog();
		exit(EXIT_FAILURE);
	}

	// Start service requests on the D-Bus
	thd_log_debug("Start main loop\n");
	g_main_loop_run(main_loop);
	thd_log_warn("Oops g main loop exit..\n");
	return THD_SUCCESS;
}

void sig_int_handler(int signum) {
	// control+c handler
	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
	if (g_main_loop)
		g_main_loop_quit(g_main_loop);
}

// main function
int main(int argc, char *argv[]) {
	gboolean show_version = FALSE;
	gboolean log_info = FALSE;
	gboolean log_debug = FALSE;
	gboolean no_daemon = FALSE;
	gboolean test_mode = FALSE;
	gint poll_interval = -1;
	gboolean success;
	GOptionContext *opt_ctx;

	thd_daemonize = TRUE;
	dbus_enable = FALSE;

	GOptionEntry options[] = { { "version", 0, 0, G_OPTION_ARG_NONE,
			&show_version, N_("Print thermald version and exit"), NULL }, {
			"no-daemon", 0, 0, G_OPTION_ARG_NONE, &no_daemon, N_(
					"Don't become a daemon: Default is daemon mode"), NULL }, {
			"loglevel=info", 0, 0, G_OPTION_ARG_NONE, &log_info, N_(
					"log severity: info level and up"), NULL }, {
			"loglevel=debug", 0, 0, G_OPTION_ARG_NONE, &log_debug, N_(
					"log severity: debug level and up: Max logging"), NULL }, {
			"test-mode", 0, 0, G_OPTION_ARG_NONE, &test_mode, N_(
					"Test Mode only: Allow non root user"), NULL }, {
			"poll-interval", 0, 0, G_OPTION_ARG_INT, &poll_interval,
			N_("Poll interval in seconds: Poll for zone temperature changes. "
					"If want to disable polling set to zero."), NULL }, {
			"dbus-enable", 0, 0, G_OPTION_ARG_NONE, &dbus_enable, N_(
					"Enable Dbus."), NULL }, { "exclusive-control", 0, 0,
			G_OPTION_ARG_NONE, &exclusive_control, N_(
					"Take over thermal control from kernel thermal driver."),
			NULL }, { "ingore-cpuid-check", 0, 0, G_OPTION_ARG_NONE,
			&ignore_cpuid_check, N_("Ignore CPU ID check."), NULL },

	{ NULL } };

	if (!g_module_supported()) {
		fprintf(stderr, "GModules are not supported on your platform!\n");
		exit(EXIT_FAILURE);
	}

	/* Set locale to be able to use environment variables */
	setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, TDLOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	/* Parse options */
	opt_ctx = g_option_context_new(NULL);
	g_option_context_set_translation_domain(opt_ctx, GETTEXT_PACKAGE);
	g_option_context_set_ignore_unknown_options(opt_ctx, FALSE);
	g_option_context_set_help_enabled(opt_ctx, TRUE);
	g_option_context_add_main_entries(opt_ctx, options, NULL);

	g_option_context_set_summary(opt_ctx,

	"Thermal daemon monitors temperature sensors and decides the best action "
			"based on the temperature readings and user preferences.");

	success = g_option_context_parse(opt_ctx, &argc, &argv, NULL);
	g_option_context_free(opt_ctx);

	if (!success) {
		fprintf(stderr,

		"Invalid option.  Please use --help to see a list of valid options.\n");
		exit(EXIT_FAILURE);
	}

	if (show_version) {
		fprintf(stdout, TD_DIST_VERSION "\n");
		exit(EXIT_SUCCESS);
	}

	if (getuid() != 0 && !test_mode) {
		fprintf(stderr, "You must be root to run thermald!\n");
		exit(EXIT_FAILURE);
	}
	if (g_mkdir_with_parents(TDRUNDIR, 0755) != 0) {
		fprintf(stderr, "Cannot create '%s': %s", TDRUNDIR, strerror(errno));
		exit(EXIT_FAILURE);
	}
	g_mkdir_with_parents(TDCONFDIR, 0755); // Don't care return value as directory
	// may already exist
	if (log_info) {
		thd_log_level |= G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;
	}
	if (log_debug) {
		thd_log_level |= G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO
				| G_LOG_LEVEL_DEBUG;
	}
	if (poll_interval >= 0) {
		fprintf(stdout, "Polling enabled: %d\n", poll_interval);
		thd_poll_interval = poll_interval;
	}

	openlog("thermald", LOG_PID, LOG_USER | LOG_DAEMON | LOG_SYSLOG);
	// Don't care return val
	//setlogmask(LOG_CRIT | LOG_ERR | LOG_WARNING | LOG_NOTICE | LOG_DEBUG | LOG_INFO);
	thd_daemonize = !no_daemon;
	g_log_set_handler(NULL, G_LOG_LEVEL_MASK, thd_logger, NULL);

	if (no_daemon)
		signal(SIGINT, sig_int_handler);

	// dbus glib processing begin
	thd_dbus_server_proc(no_daemon);

	fprintf(stdout, "Exiting ..\n");
	closelog();
}
