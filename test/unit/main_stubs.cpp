/*
 * Globals that src/main.cpp defines for the daemon, for test programs
 * that link thermald's other sources.
 */

#include <glib.h>

int thd_poll_interval = 4;
bool thd_ignore_default_control = false;
bool workaround_enabled = false;
bool disable_active_power = false;
bool ignore_critical = false;
bool power_floor_enable = false;
bool adaptive_perf_enable = true;
gboolean exclusive_control = FALSE;

/* Set THD_TEST_DEBUG=1 for thermald's debug messages */
gboolean log_debug = FALSE;
gint own_id = 0;

/* main.cpp schedules the daemon's shutdown; nothing to stop here */
void thd_request_shutdown() {}
