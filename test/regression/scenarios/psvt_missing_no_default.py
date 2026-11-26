"""A target naming a missing PSVT, and no default PSVT: thermald gives up
adaptive mode as before (ignore_adaptive, exit status 1), and says why.
See psvt_missing_fallback.
"""

import os

import gddv
from platforms import POWER_ON_PL1_W, ppcc, rapl

FLAG = "/var/run/thermald/ignore_adaptive"

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"Perf": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 90, limit="MIN")]},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Normal", gddv.IETM, "PSVT", "psvt_one")],
    },
}


def run(g):
    g.wait_until(lambda: not g.thermald_running(), 30,
                 "thermald to give up adaptive mode")
    log = g.log()
    g.check("PSVT psvt_one not found in GDDV, and no default PSVT" in log,
            "missing default PSVT not logged")
    g.check("Adaptive policy couldn't create any zones" in log,
            "no zones message missing")
    g.check(g.proc.returncode == 1, "exit status %s, expected 1"
            % g.proc.returncode)
    g.check(os.path.exists(FLAG), "%s not created" % FLAG)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
