"""The adaptive policy can't create any zones: thermald writes
/var/run/thermald/ignore_adaptive and exits, and the next start falls
back to non-adaptive mode.

The only target installs a PSVT whose sensor (SENX) doesn't exist.
"""

import os

import gddv
from platforms import ppcc, rapl

FLAG = "/var/run/thermald/ignore_adaptive"

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"broken": [gddv.psv(gddv.TCPU, "\\_SB_.SENX", 50)]},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Broken", gddv.IETM, "PSVT", "broken")],
    },
}


def run(g):
    g.wait_until(lambda: not g.thermald_running(), 30,
                 "thermald to exit when no zone can be created")
    g.check(g.proc.returncode == 1, "exit status %s, expected 1"
            % g.proc.returncode)
    g.check("Adaptive policy couldn't create any zones" in g.log(),
            "no zones message missing")
    g.check(os.path.exists(FLAG), "%s not created" % FLAG)

    # Second start, as systemd would restart it: non-adaptive mode
    g.start_thermald()
    g.wait_ready()
    g.hold(lambda: g.thermald_running(), 8, "thermald exited on restart")
    g.check(g.log().count("--adaptive option failed") >= 1,
            "restart didn't fall back to non-adaptive mode")
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s after SIGTERM" % status)
