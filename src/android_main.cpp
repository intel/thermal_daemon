/*
 * android_main.cpp: Thermal Daemon entry point tuned for Android
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include <dbus/dbus.h>
#include "thermald.h"
#include "thd_preference.h"
#include "thd_engine.h"
#include "thd_engine_therm_sys_fs.h"
#include "thd_engine_dts.h"
#include "thd_parse.h"

#define THERMALD_DAEMON_NAME	"thermald"

// poll mode
int thd_poll_interval = 4; //in seconds
static int pid_file_handle;

// Thermal engine
cthd_engine *thd_engine;
static DBusConnection	*dbus_conn;

// Start dbus server
static int thermald_dbus_server_start()
{
	DBusError err;
	int ret;
	char* param;

	dbus_error_init(&err);

	dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err)) {
		thd_log_fatal("Connection Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (NULL == dbus_conn) {
		thd_log_fatal("Connection Null\n");
		exit(1);
	}

	ret = dbus_bus_request_name(dbus_conn, THD_SERVICE_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
		thd_log_fatal("Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
		thd_log_fatal("Not Primary Owner (%d)\n", ret);
		exit(1);
	}
	thd_log_info("thermald dbus server started \n");

	return 0;
}

// Stop dbus server
static void thermald_dbus_server_stop()
{
	if (dbus_conn)
		dbus_connection_close(dbus_conn);
}

// Stop daemon
static void daemonShutdown()
{
	if (dbus_conn)
		thermald_dbus_server_stop();
	if (pid_file_handle)
		close(pid_file_handle);
}

// Stop daemon
static void thermald_dbus_listen()
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter args;
	const char *method;

	while (true) {
		bool read_msg = true;
		// Blocking read of the next available message
		dbus_connection_read_write(dbus_conn, -1);
		thd_log_debug("Got some dbus message... \n");

		while (read_msg) {
			msg = dbus_connection_pop_message(dbus_conn);

			// loop again if we haven't got a message
			if (NULL == msg) {
				read_msg = false;
				break;
			}
			method = dbus_message_get_member(msg);
			if (!method) {
				dbus_message_unref(msg);
				read_msg = false;
				break;
			}
			thd_log_info("Received dbus msg %s\n", method);

			if (!strcasecmp(method, "SetUserSetPoint")) {
				bool status;
				char *set_point;

				DBusError error;

				dbus_error_init(&error);
				if (dbus_message_get_args(msg, &error,
								DBUS_TYPE_STRING, &set_point, DBUS_TYPE_INVALID)) {
					thd_log_info("New Set Point %s\n", set_point);
					cthd_preference thd_pref;
					if (thd_engine->thd_engine_set_user_set_point((char*)set_point) == THD_SUCCESS)
						thd_engine->send_message(PREF_CHANGED, 0, NULL);
				}
				else {
					thd_log_error("dbus_message_get_args failed %s\n", error.message);
				}
				dbus_error_free(&error);
			}
			else if (!strcasecmp(method, "CalibrateStart")) {
				// TBD
			}
			else if (!strcasecmp(method, "CalibrateEnd")) {
				// TBD
			}
			else if (!strcasecmp(method, "Terminate")) {
				daemonShutdown();
				exit(EXIT_SUCCESS);
			}
			else {
				thd_log_error("dbus_message_get_args Invalid Message\n");
			}

			// free the message
			dbus_message_unref(msg);
		}
	}
}

// signal handler
static void signal_handler(int sig)
{
	switch(sig) {
		case SIGHUP:
			thd_log_warn("Received SIGHUP signal.");
			break;
		case SIGINT:
		case SIGTERM:
			thd_log_info("Daemon exiting");
			daemonShutdown();
			exit(EXIT_SUCCESS);
			break;
		default:
			thd_log_warn("Unhandled signal %s", strsignal(sig));
			break;
	}
}

static void daemonize(char *rundir, char *pidfile)
{
	int pid, sid, i;
	char str[10];
	struct sigaction sig_actions;
	sigset_t sig_set;

	if (getppid() == 1) {
		return;
	}
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGCHLD);
	sigaddset(&sig_set, SIGTSTP);
	sigaddset(&sig_set, SIGTTOU);
	sigaddset(&sig_set, SIGTTIN);
	sigprocmask(SIG_BLOCK, &sig_set, NULL);

	sig_actions.sa_handler = signal_handler;
	sigemptyset(&sig_actions.sa_mask);
	sig_actions.sa_flags = 0;

	sigaction(SIGHUP, &sig_actions, NULL);
	sigaction(SIGTERM, &sig_actions, NULL);
	sigaction(SIGINT, &sig_actions, NULL);

	pid = fork();
	if (pid < 0) {
		/* Could not fork */
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		thd_log_info("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}
	umask(027);

	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}
	/* close all descriptors */
	for (i = getdtablesize(); i >= 0; --i) {
		close(i);
	}

	i = open("/dev/null", O_RDWR);
	dup(i);
	dup(i);
	chdir(rundir);

	pid_file_handle = open(pidfile, O_RDWR|O_CREAT, 0600);
	if (pid_file_handle == -1 )
	{
		/* Couldn't open lock file */
		thd_log_info("Could not open PID lock file %s, exiting", pidfile);
		exit(1);
	}
	/* Try to lock file */
	if (flock(pid_file_handle,LOCK_EX|LOCK_NB) < 0)
	{
		/* Couldn't get lock on lock file */
		thd_log_info("Couldn't get lock file %d\n", getpid());
		exit(1);
	}
	thd_log_info("Thermal PID %d\n", getpid());
	sprintf(str,"%d\n",getpid());
	write(pid_file_handle, str, strlen(str));
}

static void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  dptf_if options [ ... ]\n");
  fprintf (stream,
		"  --help	Display this usage information.\n"
		"  --version Show version.\n"
		"  --no-daemon No daemon.\n"
		"  --poll-interval poll interval 0 to disable.\n"
		"  --dbus-enable Enable dbus I/F.\n"
		"  --use-thermal-sysfs to act as thermal controller. \n");

	exit (exit_code);
}

int main(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	bool no_daemon = false;
	bool use_thermal_sys_fs = false;
	bool dbus_enable = false;
	const char* const short_options = "hv:np:dt";
	static struct option long_options[] = {
		{"help", 0, 0, 0},
		{"version", 0, 0, 0},
		{"no-daemon", 0, 0, 0},
		{"poll-interval",4, 0, 0},
		{"dbus-enable", 0, 0, 0},
		{"use-thermal-sysfs", 0, 0, 0},
		{NULL, 0, NULL, 0}
	};

	if (argc > 1 ) {
		while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
			int this_option_optind = optind ? optind : 1;
			switch(c) {
			case 'h':
				print_usage (stdout, 0);
				break;
			case 'v':
				fprintf(stdout, "1.03-01\n");
				exit(0);
				break;
			case 'n':
				no_daemon = true;
				break;
			case 'd':
				dbus_enable = true;
				break;
			case -1:
			case 0:
				break;
			default:
				break;
			}
		}
	}
	if(getuid() != 0)
	{
		fprintf(stderr, "You must be root to run thermal daemon!\n");
		exit(1);
	}

	if((c = mkdir(TDRUNDIR, 0755)) != 0)
	{
		if (errno != EEXIST) {
			fprintf(stderr, "Cannot create '%s': %s\n", TDRUNDIR, strerror(errno));
			exit(1);
		}
	}
	mkdir(TDCONFDIR, 0755); // Don't care return value as directory
	if (!no_daemon)
		daemonize((char *)"/tmp/", (char *)"/tmp/thermald.pid");

	thd_log_info("Linux Thermal Daemon is starting mode %d \n", no_daemon);

	if(use_thermal_sys_fs)
		thd_engine = new cthd_engine_therm_sysfs();
	else
	{
		cthd_parse parser;
		bool matched = false;

		// if there is XML config for this platform
		// Use this instead of default DTS sensor and associated cdevs
		if(parser.parser_init() == THD_SUCCESS)
		{
			if(parser.start_parse() == THD_SUCCESS)
			{
				matched = parser.platform_matched();
			}
			parser.parser_deinit();
		}
		if (matched) {
			thd_log_warn("UUID/Name matched, so will load zones and cdevs from thermal-conf.xml\n");
			thd_engine = new cthd_engine_therm_sysfs();
		}
		else
			thd_engine = new cthd_engine_dts();
	}
	// Initialize thermald objects
	if(thd_engine->thd_engine_start(false) != THD_SUCCESS)
	{
		thd_log_error("thermald engine start failed: ");
		exit(1);
	}

	if (dbus_enable) {
		if (thermald_dbus_server_start()) {
			fprintf(stderr, "Dbus start error\n");
			exit(1);
		}
		thermald_dbus_listen(); // Will block here
		thermald_dbus_server_stop();
	} else {
		for (;;)
			sleep(0xffff);
	}
	thd_log_info("Linux Thermal Daemon is exiting \n");

	return 0;
}
