"""An ITMT table is used only when the IDSP lists the ITMT UUID.

Same vault as itmt_throttle_restore but without the UUID. thermald
must skip the table (cthd_gddv::find_itmt(): "no IDSP support"). As the
ITMT target is the only policy, no zone is created: thermald writes
ignore_adaptive and exits with status 1, restoring the power-on PL1, as
in ignore_adaptive_flow. It must not throttle on the skipped table
before that.
"""

import os

import gddv
from platforms import POWER_ON_PL1_W, ppcc, rapl

FLAG = "/var/run/thermald/ignore_adaptive"

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 75},       # above the ITMT trip from the start
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 25000, "MAX")]},
    },
}


def run(g):
    g.wait_until(lambda: not g.thermald_running(), 30,
                 "thermald to exit when the ITMT table can't be used")
    log = g.log()
    g.check("set_int3400 ITMT target IETM.D0" in log, "ITMT target not selected")
    g.check("no IDSP support" in log, "ITMT table not skipped for the missing IDSP")
    g.check("Adaptive policy couldn't create any zones" in log,
            "no zones message missing")
    g.check(g.proc.returncode == 1, "exit status %s, expected 1"
            % g.proc.returncode)
    g.check(os.path.exists(FLAG), "%s not created" % FLAG)
    throttled = [w["value"] / 1e6 for w in g.rapl_writes("mmio")
                 if w["value"] / 1e6 < POWER_ON_PL1_W]
    g.check(not throttled, "PL1 throttled by the skipped table: %s" % throttled)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
