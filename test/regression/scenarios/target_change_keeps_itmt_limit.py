"""A change of the adaptive target while an ITMT trip limits PL1 keeps the
limit until the sensor is below that trip minus its hysteresis, then
releases it gradually.

Condition sets: SEN3 >= 59 C selects "Hot", else "Cool". ITMT for SEN2,
PL1 down to 25 W: "Cool" at 60 C (written as 59.5 C), "Hot" at 70 C.
PPCC: PL1 10-35 W.

  1. SEN2 45 C: Cool, PL1 35 W.
  2. SEN2 65 C: Cool limits PL1 to 25 W.
  3. SEN3 60 C: the target changes to Hot, whose 70 C trip doesn't limit
     at 65 C; PL1 stays at 25 W while SEN2 is at or above 57.5 C.
  4. SEN2 55 C, below 57.5 C but above the zone's polling trip (90 % of
     the lowest trip; below it the zone is reset as before): PL1 released
     to 35 W in several steps, and the carried limit is gone.
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 45, "SEN3": 45},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(2, [gddv.temp_cond("\\_SB_.SEN3", gddv.GREATER_OR_EQUAL, 59)]),
                 (1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Cool", gddv.IETM, "ITMT", "itmA"),
                 gddv.action(2, "Hot", gddv.IETM, "ITMT", "itmB")],
        "itmt": {"itmA": [("\\_SB_.SEN2", 60, 25000, "MAX")],
                 "itmB": [("\\_SB_.SEN2", 70, 25000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def pl1(g):
    return g.pl1("mmio")


def run(g):
    g.wait_ready()
    g.wait_until(lambda: pl1(g) == 35, 30, lambda: "SEN2 45 C: PL1 35 W (is %.1f W)" % pl1(g))
    g.set_temp("SEN2", 65)
    g.wait_until(lambda: pl1(g) == 25, 60, lambda: "SEN2 65 C: PL1 25 W (is %.1f W)" % pl1(g))

    g.set_temp("SEN3", 60)
    g.wait_log("Target Name:Hot")
    g.hold(lambda: pl1(g) <= 25.01, 12,
           lambda: "target Hot, SEN2 still 65 C: PL1 released to %.1f W" % pl1(g))
    g.check("target change: keep" in g.log(), "limit not carried over the target change")

    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 55)
    g.wait_until(lambda: pl1(g) == 35, 60, lambda: "SEN2 55 C: PL1 back at 35 W (is %.1f W)" % pl1(g))
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][n - 1:]
    g.check(len(set(v for v in w if 25 < v < 35)) >= 2, "not released gradually: %s" % w)
    g.wait_log("carried over the target change released", timeout=10)
    g.check(g.stop_thermald() == 0, "exit status")
