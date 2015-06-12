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
 * which parses command line arguments, setup logs and starts thermal
 * engine.
 */

#include "thermald.h"
#include "thd_preference.h"
#include "thd_engine.h"
#include "thd_engine_default.h"
#include "thd_parse.h"

// poll mode
int thd_poll_interval = 4; //in seconds
static int pid_file_handle;

// Stop daemon
static void daemonShutdown() {
	if (pid_file_handle)
		close(pid_file_handle);
	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
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
		thd_log_warn("Unhandled signal %s\n", strsignal(sig));
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
		thd_log_info("Could not open PID lock file %s, exiting\n", pidfile);
		exit(EXIT_FAILURE);
	}
	/* Try to lock file */
#ifdef LOCKF_SUPPORT
	if (lockf(pid_file_handle, F_TLOCK, 0) == -1) {
#else
		if (flock(pid_file_handle,LOCK_EX|LOCK_NB) < 0) {
#endif
		/* Couldn't get lock on lock file */
		thd_log_info("Couldn't get lock file %d\n", getpid());
		exit(EXIT_FAILURE);
	}
	thd_log_info("Thermal PID %d\n", getpid());
	snprintf(str, sizeof(str), "%d\n", getpid());
	write(pid_file_handle, str, strlen(str));
}

static void print_usage(FILE* stream, int exit_code) {
	fprintf(stream, "Usage:  thermal-daemon options [ ... ]\n");
	fprintf(stream, "  --help Display this usage information.\n"
			"  --version Show version.\n"
			"  --no-daemon No daemon.\n"
			"  --poll-interval poll interval 0 to disable.\n"
			"  --exclusive_control to act as exclusive thermal controller. \n");

	exit(exit_code);
}

int main(int argc, char *argv[]) {
	int c;
	int option_index = 0;
	bool no_daemon = false;
	bool exclusive_control = false;
	bool test_mode = false;

	const char* const short_options = "hvnp:de";
	static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'v' },
			{ "no-daemon", no_argument, 0, 'n' },
			{ "poll-interval", required_argument, 0, 'p' },
			{ "exclusive_control", no_argument, 0, 'e' },
			{ "test-mode", no_argument, 0, 't' },
			{ NULL, 0, NULL, 0 } };

	if (argc > 1) {
		while ((c = getopt_long(argc, argv, short_options, long_options,
				&option_index)) != -1) {
			int this_option_optind = optind ? optind : 1;
			switch (c) {
			case 'h':
				print_usage(stdout, 0);
				break;
			case 'v':
				fprintf(stdout, "1.1\n");
				exit(EXIT_SUCCESS);
				break;
			case 'n':
				no_daemon = true;
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
		exit(EXIT_FAILURE);
	}

	if ((c = mkdir(TDRUNDIR, 0755)) != 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Cannot create '%s': %s\n", TDRUNDIR,
					strerror(errno));
			exit(EXIT_FAILURE);
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

	if (thd_engine_create_default_engine(false,
			exclusive_control) != THD_SUCCESS) {
		exit(EXIT_FAILURE);
	}

#ifdef VALGRIND_TEST
	// lots of STL lib function don't free memory
	// when called with exit().
	// Here just run for some time and gracefully return.
	sleep(10);
	if (pid_file_handle)
		close(pid_file_handle);
	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
#else
	for (;;)
		sleep(0xffff);

	thd_log_info("Linux Thermal Daemon is exiting \n");
#endif
	return 0;
}
