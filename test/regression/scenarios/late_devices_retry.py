"""Thermal devices that appear after thermald started (boot race).

At boot, drivers such as proc_thermal and intel_rapl_msr can probe after
thermald has read the thermal sysfs. The PSVT then gets no zone. Here the
test module (SEN2 and RAPL) is loaded only after thermald has started:
thermald must exit with adaptive_retried instead of ignore_adaptive, and
the restart must run adaptive mode and set PL1 to the PPCC maximum. It
retries only once per boot: the next time, it writes ignore_adaptive.
"""

import os

import gddv
from platforms import base_vault, pl1max_target, rapl

FLAG = "/var/run/thermald/ignore_adaptive"
RETRIED = "/var/run/thermald/adaptive_retried"
PPCC_MAX_W = 35

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": base_vault(apct=[(1, [gddv.cond(gddv.DEFAULT)])],
                       apat=pl1max_target(1, "Default", PPCC_MAX_W)),
    # The first target is applied one poll interval after the engine
    # thread starts: the time to load the module
    "thermald_args": ["--adaptive", "--poll-interval=3"],
    "autostart": False,
}


def run(g):
    g.unload_module()
    g.start_thermald()
    g.wait_log("thd_engine_thread begin")
    g.load_module()

    g.wait_until(lambda: not g.thermald_running(), 30,
                 "thermald to exit when no zone can be created")
    g.check(g.proc.returncode == 1, "exit status %s, expected 1"
            % g.proc.returncode)
    g.check("Thermal devices appeared since startup" in g.log(),
            "no retry message")
    g.check(not os.path.exists(FLAG), "%s created" % FLAG)
    g.check(os.path.exists(RETRIED), "%s not created" % RETRIED)

    # Second start, as systemd would restart it: adaptive mode
    g.start_thermald()
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 20,
                 lambda: "MMIO PL1 at the PPCC maximum %d W (is %.1f W)"
                 % (PPCC_MAX_W, g.pl1("mmio")))
    g.check("--adaptive option failed" not in g.log(),
            "restart fell back to non-adaptive mode")
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s after SIGTERM" % status)

    # Only once per boot: the next time, fall back
    g.unload_module()
    g.start_thermald()
    g.wait_until(lambda: g.log().count("thd_engine_thread begin") == 3, 30,
                 "engine thread of the third start")
    g.load_module()
    g.wait_until(lambda: not g.thermald_running(), 30,
                 "thermald to exit when no zone can be created")
    g.check(os.path.exists(FLAG), "%s not created after a retry" % FLAG)
