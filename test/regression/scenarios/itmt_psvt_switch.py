"""Switching between an ITMT target and a PSVT target replaces the trips.

Condition sets: power slider "power-saver" (25) selects "Psv", a PSVT
target throttling on SEN2 at 50 C; anything else selects "Itmt", an
ITMT target throttling on TCPU at 60 C (PL1 20 W). Both CPU zones are
kept hot in turn; only the selected target's trip may limit PL1.

  1. balanced, Itmt: TCPU 65 C throttles to 20 W; SEN2 60 C doesn't.
  2. power-saver, Psv: TCPU 65 C no longer throttles (PL1 back at 35 W);
     SEN2 60 C does.
  3. balanced again, Itmt: same as 1.
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["TCPU", "SEN2"],
    "initial_temps": {"TCPU": 45, "SEN2": 35},
    "rapl": rapl(),
    "power_profile": "balanced",
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"Psv": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit="MIN")]},
        "apct": [
            (2, [gddv.cond(gddv.POWER_SLIDER, gddv.EQUAL, 25)]),
            (1, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0"),
                 gddv.action(2, "Psv", gddv.IETM, "PSVT", "Psv")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 20000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def pl1(g):
    return g.pl1("mmio")


def itmt_phase(g, what):
    g.set_temp("SEN2", 35)
    g.set_temp("TCPU", 65)
    g.wait_until(lambda: pl1(g) == 20, 60,
                 lambda: "%s: TCPU 65 C, ITMT PL1 20 W (is %.1f W)" % (what, pl1(g)))
    g.set_temp("TCPU", 45)
    g.wait_until(lambda: pl1(g) == 35, 60,
                 lambda: "%s: TCPU cool, PL1 35 W (is %.1f W)" % (what, pl1(g)))
    g.set_temp("SEN2", 60)
    g.hold(lambda: pl1(g) == 35, 15,
           lambda: "%s: SEN2 60 C throttled (PL1 %.1f W), but the PSVT "
           "target isn't selected" % (what, pl1(g)))
    g.set_temp("SEN2", 35)


def run(g):
    g.wait_ready()
    g.wait_log("Target Name:Itmt")
    g.wait_until(lambda: pl1(g) == 35, 30, "PL1 at 35 W at start")
    itmt_phase(g, "Itmt")

    g.set_power_profile("power-saver")
    g.wait_log("Target Name:Psv")
    g.set_temp("TCPU", 65)
    g.hold(lambda: pl1(g) == 35, 15,
           lambda: "Psv: TCPU 65 C throttled (PL1 %.1f W): the ITMT trip "
           "was kept after switching to the PSVT target" % pl1(g))
    g.set_temp("TCPU", 45)
    g.set_temp("SEN2", 60)
    g.wait_until(lambda: pl1(g) <= 15, 90,
                 lambda: "Psv: SEN2 60 C throttles PL1 (is %.1f W)" % pl1(g))
    g.set_temp("SEN2", 35)
    g.wait_until(lambda: pl1(g) == 35, 90,
                 lambda: "Psv: SEN2 cool, PL1 35 W (is %.1f W)" % pl1(g))

    g.set_power_profile("balanced")
    g.wait_until(lambda: g.log().count("Target Name:Itmt") >= 2, 20,
                 "Itmt selected again")
    itmt_phase(g, "Itmt again")

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
