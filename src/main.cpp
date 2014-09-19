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

extern int thd_dbus_server_init();

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

static GMainLoop *g_main_loop;

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

bool check_thermald_running() {
	const char *lock_file = TDRUNDIR
	"/thermald.pid";
	int pid_file_handle;

	pid_file_handle = open(lock_file, O_RDWR | O_CREAT, 0600);
	if (pid_file_handle == -1) {
		/* Couldn't open lock file */
		thd_log_error("Could not open PID lock file %s, exiting\n", lock_file);
		return false;
	}
	/* Try to lock file */
	if (lockf(pid_file_handle, F_TLOCK, 0) == -1) {
		/* Couldn't get lock on lock file */
		thd_log_error("Couldn't get lock file %d\n", getpid());
		return true;
	}

	return false;
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

	if (check_thermald_running()) {
		thd_log_fatal(
				"An instance of thermald is already running, exiting ...\n");
		exit(EXIT_FAILURE);
	}

	if (no_daemon)
		signal(SIGINT, sig_int_handler);

	// Initialize the GType/GObject system
	g_type_init();

	// Create a main loop that will dispatch callbacks
	g_main_loop = g_main_loop_new(NULL, FALSE);
	if (g_main_loop == NULL) {
		thd_log_error("Couldn't create GMainLoop:\n");
		return THD_FATAL_ERROR;
	}

	if (dbus_enable)
		thd_dbus_server_init();

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
	g_main_loop_run(g_main_loop);
	thd_log_warn("Oops g main loop exit..\n");

	fprintf(stdout, "Exiting ..\n");
	closelog();
}
