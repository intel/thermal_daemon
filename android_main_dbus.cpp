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
#include "thd_engine_default.h"
#include "thd_parse.h"

#define THERMALD_DAEMON_NAME	"thermald"

// poll mode
int thd_poll_interval = 4; //in seconds
static int pid_file_handle;

// Thermal engine
cthd_engine *thd_engine;
static DBusConnection *dbus_conn;

// Start dbus server
static int thermald_dbus_server_start() {
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

	ret = dbus_bus_request_name(dbus_conn, THD_SERVICE_NAME,
			DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
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
static void thermald_dbus_server_stop() {
	if (dbus_conn)
		dbus_connection_close(dbus_conn);
}

// Stop daemon
static void daemonShutdown() {
	if (dbus_conn)
		thermald_dbus_server_stop();
	if (pid_file_handle)
		close(pid_file_handle);
	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
}

// Stop daemon
static void thermald_dbus_listen() {
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

			if (!strcasecmp(method, "SetUserMaxTemperature")) {
				bool status;
				char *zone_name;
				char *set_point;
				DBusError error;

				dbus_error_init(&error);
				if (dbus_message_get_args(msg, &error, DBUS_TYPE_STRING,
						&zone_name, DBUS_TYPE_STRING, &set_point,
						DBUS_TYPE_INVALID)) {
					thd_log_info("New Set Point %s\n", set_point);
					cthd_preference thd_pref;
					if (thd_engine->thd_engine_set_user_max_temp(zone_name,
							set_point) == THD_SUCCESS)
						thd_engine->send_message(PREF_CHANGED, 0, NULL);
				} else {
					thd_log_error("dbus_message_get_args failed %s\n",
							error.message);
				}
				dbus_error_free(&error);
			} else if (!strcasecmp(method, "SetCurrentPreference")) {
				bool status;
				char *pref_str;
				DBusError error;

				dbus_error_init(&error);
				if (dbus_message_get_args(msg, &error, DBUS_TYPE_STRING,
						&pref_str, DBUS_TYPE_INVALID)) {
					thd_log_info("New Pref %s\n", pref_str);
					cthd_preference thd_pref;
					thd_pref.set_preference(pref_str);
					thd_engine->send_message(PREF_CHANGED, 0, NULL);
				} else {
					thd_log_error("dbus_message_get_args failed %s\n",
							error.message);
				}
				dbus_error_free(&error);
			} else if (!strcasecmp(method, "CalibrateStart")) {
				// TBD
			} else if (!strcasecmp(method, "CalibrateEnd")) {
				// TBD
			} else if (!strcasecmp(method, "Terminate")) {
				daemonShutdown();
				exit(EXIT_SUCCESS);
			} else {
				thd_log_error("dbus_message_get_args Invalid Message\n");
			}

			// free the message
			dbus_message_unref(msg);
		}
	}
}

// signal handler
static void signal_handler(int sig) {
	switch (sig) {
	case SIGHUP:
		thd_log_warn("Received SIGHUP signal. \n");
		break;
	case SIGINT:
	case SIGTERM:
		thd_log_info("Daemon exiting \n");
		daemonShutdown();
		exit(EXIT_SUCCESS);
		break;
	default:
		thd_log_warn("Unhandled signal %s", strsignal(sig));
		break;
	}
}

static void daemonize(char *rundir, char *pidfile) {
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

	pid_file_handle = open(pidfile, O_RDWR | O_CREAT, 0600);
	if (pid_file_handle == -1) {
		/* Couldn't open lock file */
		thd_log_info("Could not open PID lock file %s, exiting", pidfile);
		exit(1);
	}
	/* Try to lock file */
#ifdef LOCKF_SUPPORT
	if (lockf(pid_file_handle, F_TLOCK, 0) == -1) {
#else
		if (flock(pid_file_handle,LOCK_EX|LOCK_NB) < 0) {
#endif
		/* Couldn't get lock on lock file */
		thd_log_info("Couldn't get lock file %d\n", getpid());
		exit(1);
	}
	thd_log_info("Thermal PID %d\n", getpid());
	sprintf(str, "%d\n", getpid());
	write(pid_file_handle, str, strlen(str));
}

static void print_usage(FILE* stream, int exit_code) {
	fprintf(stream, "Usage:  dptf_if options [ ... ]\n");
	fprintf(stream, "  --help	Display this usage information.\n"
			"  --version Show version.\n"
			"  --no-daemon No daemon.\n"
			"  -poll-interval poll interval 0 to disable.\n"
			"  --dbus-enable Enable dbus I/F.\n"
			"  --exclusive_control to act as exclusive thermal controller. \n");

	exit(exit_code);
}

int main(int argc, char *argv[]) {
	int c;
	int option_index = 0;
	bool no_daemon = false;
	bool exclusive_control = false;
	bool dbus_enable = false;
	bool test_mode = false;

	const char* const short_options = "hvnp:de";
	static struct option long_options[] = { { "help", no_argument, 0, 'h' }, {
			"version", no_argument, 0, 'v' },
			{ "no-daemon", no_argument, 0, 'n' }, { "poll-interval",
					required_argument, 0, 'p' }, { "dbus-enable", no_argument,
					0, 'd' }, { "exclusive_control", no_argument, 0, 'e' }, {
					"test-mode", no_argument, 0, 't' }, { NULL, 0, NULL, 0 } };

	if (argc > 1) {
		while ((c = getopt_long(argc, argv, short_options, long_options,
				&option_index)) != -1) {
			int this_option_optind = optind ? optind : 1;
			switch (c) {
			case 'h':
				print_usage(stdout, 0);
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
			case 'p':
				thd_poll_interval = atoi(optarg);
				break;
			case 'e':
				exclusive_control = true;
				break;
			case 't':
				test_mode = true;
				break;
			case -1:
			case 0:
				break;
			default:
				break;
			}
		}
	}
	if (getuid() != 0 && !test_mode) {
		fprintf(stderr, "You must be root to run thermal daemon!\n");
		exit(1);
	}

	if ((c = mkdir(TDRUNDIR, 0755)) != 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Cannot create '%s': %s\n", TDRUNDIR,
					strerror(errno));
			exit(1);
		}
	}
	mkdir(TDCONFDIR, 0755); // Don't care return value as directory
	if (!no_daemon) {
		daemonize((char *) "/tmp/", (char *) "/tmp/thermald.pid");
	} else
		signal(SIGINT, signal_handler);

	thd_log_info(
			"Linux Thermal Daemon is starting mode %d : poll_interval %d :ex_control %d\n",
			no_daemon, thd_poll_interval, exclusive_control);

	thd_engine = new cthd_engine_default();
	if (exclusive_control)
		thd_engine->set_control_mode(EXCLUSIVE);

	thd_engine->set_poll_interval(thd_poll_interval);
	if (thd_engine->thd_engine_start(false) != THD_SUCCESS) {
		thd_log_error("THD engine start failed: ");
		exit(1);
	}

	// Initialize thermald objects
	if (thd_engine->thd_engine_start(false) != THD_SUCCESS) {
		thd_log_error("thermald engine start failed: ");
		exit(1);
	}
#ifdef VALGRIND_TEST
	// lots of STL lib function don't free memory
	// when called with exit() fn.
	// Here just run for some time and gracefully return.
	sleep(10);
	if (dbus_conn)
	thermald_dbus_server_stop();
	if (pid_file_handle)
	close(pid_file_handle);
	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
#else
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
#endif
	return 0;
}
