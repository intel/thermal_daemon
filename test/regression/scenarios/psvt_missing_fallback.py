"""A target naming a PSVT that isn't in the GDDV uses the default PSVT.

Seen on a Dell Precision 3660: both targets name PSVTs missing from the
GDDV. thermald used to install nothing and give up adaptive mode, leaving
PL1 at its power-on value. It must install the default PSVT, IETM.D0,
instead, not another table: here "Perf", which trips at 90 C, would
throttle less than the platform's baseline policy (60 C).

  1. thermald stays adaptive and logs the fallback to IETM.D0.
  2. PL1 at the PPCC maximum, 35 W.
  3. SEN2 70 C: PL1 comes down (IETM.D0's 60 C trip, not Perf's 90 C).
  4. SEN2 40 C: PL1 back at 35 W.
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {
            "IETM.D0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 60, limit="MIN")],
            "Perf": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 90, limit="MIN")],
        },
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Normal", gddv.IETM, "PSVT", "psvt_one")],
    },
}


def pl1(g):
    return g.pl1("mmio")


def run(g):
    g.wait_ready()
    g.hold(lambda: g.thermald_running(), 5,
           lambda: "thermald exited (status %s): adaptive mode gave up"
           % g.proc.returncode)
    g.check("PSVT psvt_one not found in GDDV, using default PSVT IETM.D0"
            in g.log(), "fallback to IETM.D0 not logged")
    g.wait_until(lambda: pl1(g) == 35, 30,
                 lambda: "PL1 at the PPCC maximum 35 W (is %.1f W)" % pl1(g))

    g.set_temp("SEN2", 70)
    g.wait_until(lambda: pl1(g) <= 15, 90,
                 lambda: "SEN2 70 C: IETM.D0 throttles PL1 (is %.1f W)" % pl1(g))
    g.set_temp("SEN2", 40)
    g.wait_until(lambda: pl1(g) == 35, 90,
                 lambda: "SEN2 40 C: PL1 back at 35 W (is %.1f W)" % pl1(g))

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
