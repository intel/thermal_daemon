"""BIOS-locked RAPL: every power limit write fails with -ENODATA.

thermald must keep running, leave PL1 at the firmware value, notice the
lock instead of retrying every poll, and exit cleanly.
"""

from platforms import POWER_ON_PL1_W, ppcc, rapl

import gddv

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(locked=1),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [
            gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit="MIN"),
        ]},
    },
}


def failed_writes(g):
    return [w for w in g.rapl_writes(attr="constraint_0_power") if w["ret"]]


def run(g):
    g.wait_ready()
    g.set_temp("SEN2", 65)          # would throttle if it could

    g.hold(lambda: g.thermald_running(), 20, "thermald exited")
    for d in ("mmio", "msr"):
        g.check(g.pl1(d) == POWER_ON_PL1_W,
                "%s PL1 changed on a locked part: %.1f W" % (d, g.pl1(d)))
    g.check("BIOS locked" in g.log(), "the lock was not reported")

    # Each domain may try a few times before marking itself locked, but
    # must not retry every poll: count attempts in the last 10 s
    before = len(failed_writes(g))
    g.hold(lambda: True, 10, "")
    retries = len(failed_writes(g)) - before
    g.check(retries <= 2, "%d more failing PL1 writes in 10 s after the "
            "lock was detected" % retries)

    status = g.stop_thermald()
    g.check(status == 0, "thermald exit status %s" % status)
