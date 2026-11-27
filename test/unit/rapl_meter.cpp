#include "thd_rapl_power_meter.h"
#include <cassert>

static std::string root;
static long long elapsed_ms = 100900;
static long long suspended_ms;
static time_t wall = 100;

// Redirect discovery to the fixture before the meter reads any domains.
extern "C" bool __wrap__ZN7csys_fs6existsEv(csys_fs *fs) {
	fs->update_path(root);
	return true;
}

extern "C" time_t __wrap_time(time_t *value) {
	if (value)
		*value = wall;
	return wall;
}

extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec *ts) {
	long long ms = elapsed_ms + (clock == CLOCK_BOOTTIME ? suspended_ms : 0);

	ts->tv_sec = ms / 1000;
	ts->tv_nsec = ms % 1000 * 1000000;
	return 0;
}

int main(int argc, char **argv) {
	assert(argc == 2);
	root = std::string(argv[1]) + "/";
	cthd_rapl_power_meter meter(PACKAGE);
	meter.rapl_start_measure_power();
	auto sample = [&](int energy_uj) {
		std::ofstream(root + "pkg/energy_uj") << energy_uj;
		meter.rapl_measure_power();
		return meter.rapl_action_get_last_power(PACKAGE);
	};

	assert(sample(1000000) == 0);
	elapsed_ms += 1250;
	wall += 2;
	assert(sample(51000000) == 40000000);
	elapsed_ms += 100;
	assert(sample(52000000) == 40000000);
	elapsed_ms += 400;
	wall -= 300;
	assert(sample(61000000) == 20000000);
	// 50 J over one second awake and 999 s suspended
	elapsed_ms += 1000;
	suspended_ms += 999000;
	assert(sample(111000000) == 50000);
}
