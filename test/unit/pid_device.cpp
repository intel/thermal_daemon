#include "thd_trip_point.h"
#include <cassert>

class power_limit : public cthd_cdev {
public:
	power_limit() : cthd_cdev(0, "") {
	}

	void set_curr_state_raw(int state, int) override {
		curr_state = state;
	}

	void set_curr_state(int state, int) override {
		curr_state = state;
	}
};

static time_t elapsed = 100;

static void check_pid_restart(bool in_band) {
	power_limit pl1;
	pl1.set_min_state(41000000);
	pl1.set_max_state(32000000);
	pl1.set_curr_state(41000000, 0);
	pl1.set_inc_dec_value(-2000000);
	pl1.set_debounce_interval(-1);

	cthd_trip_point trip(0, PASSIVE, 52000, 2000, 0, 0);
	pid_param_t param = {};
	param.valid = param.gradual_release = param.time_scaled = 1;
	param.mode = PID_INCREMENTAL;
	param.kp = 16;
	param.kd = 500;
	param.set_point_offset = 500;
	trip.thd_trip_point_add_cdev(pl1, 0, 0, 0, 0, &param, 1, 41000000, 32000000);
	auto poll = [&](int temp) {
		bool reset;
		elapsed += 5;
		trip.thd_trip_point_check(0, temp, PREF_ENERGY_CONSERVE, &reset);
		return pl1.get_curr_state();
	};

	int first = poll(52050);
	assert(first == 41000000 - 16 * 550);
	if (in_band) {
		assert(poll(51050) == 41000000);
		assert(poll(49050) == 41000000);
		cthd_trip_point peer(0, PASSIVE, 52000, 2000, 1, 1);
		peer.thd_trip_point_add_cdev(pl1, 0, 0, 0, 0, &param, 1, 41000000, 35000000);
		bool reset;
		peer.thd_trip_point_check(1, 52050, PREF_ENERGY_CONSERVE, &reset);
		assert(pl1.get_curr_state() == first);
		peer.thd_trip_point_check(1, 48050, PREF_ENERGY_CONSERVE, &reset);
		assert(poll(52050) == first);
		return;
	}
	poll(55050);
	poll(55050);
	assert(poll(55050) == 32000000);
	assert(poll(46050) < 41000000);
	trip.thd_trip_cdev_state_reset();
	assert(pl1.get_curr_state() == 41000000);
	assert(poll(52050) == first);
}

extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec *ts) {
	assert(clock == CLOCK_MONOTONIC);
	ts->tv_sec = elapsed;
	ts->tv_nsec = 0;
	return 0;
}

int main(int argc, char **argv) {
	check_pid_restart(argc > 1 && std::string(argv[1]) == "band-release");
}
