#include "thd_pid.h"
#include <cassert>

static time_t elapsed;
static time_t wall = 1000;

extern "C" time_t __wrap_time(time_t *value) {
	if (value)
		*value = wall;
	return wall;
}

extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec *ts) {
	assert(clock == CLOCK_MONOTONIC);
	ts->tv_sec = elapsed;
	ts->tv_nsec = 0;
	return 0;
}

int main() {
	cthd_pid pid;
	pid.set_pid_param(0, 1, 0);
	pid.set_target_temp(50000);
	assert(pid.pid_output(50001) == 0);
	elapsed += 4;
	wall -= 300;
	assert(pid.pid_output(50001) == 4);
	elapsed += 4;
	wall += 3600;
	assert(pid.pid_output(50001) == 8);
	pid.reset();
	assert(pid.pid_output(50001, 2) == 2);

	pid.reset();
	pid.set_pid_mode(PID_INCREMENTAL);
	pid.set_time_scaled(true);
	pid.set_pid_param(2, 0, 3);
	assert(pid.pid_output(50001) == 2);
	elapsed += 5;
	wall -= 300;
	assert(pid.pid_output(50003) == 36);
	elapsed += 4;
	wall += 3600;
	assert(pid.pid_output(50003) == 24);
}
