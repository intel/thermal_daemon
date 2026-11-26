"""A change of the adaptive target while PSVT step control limits PL1
keeps the limit until the sensor is below the trip that limited it, then
releases it at that entry's unlimit rate.

Condition sets: SEN3 >= 59 C selects "Hot", else "Cool". PSVT for SEN2,
step 500 mW, coefficients 10/10 (5 W per 1 s): "Cool" a MAX entry at
50 C (full limit, the PPCC minimum 10 W, at 51 C), "Hot" one at 56 C.

  1. SEN2 40 C: Cool, PL1 35 W.
  2. SEN2 53 C: Cool limits PL1 to 10 W.
  3. SEN3 60 C: the target changes to Hot, whose 56 C entry doesn't limit
     at 53 C; PL1 stays at 10 W while SEN2 is at or above 50 C.
  4. SEN2 49 C: PL1 released to 35 W in steps of at most 5 W, and the
     carried limit is gone.
"""

import gddv
from platforms import pl1max_target, ppcc, rapl


def entry(celsius):
    e = gddv.psv(gddv.TCPU, "\\_SB_.SEN2", celsius, limit="MAX", sample_period=10,
                 step_size=500, limit_coeff=10, unlimit_coeff=10)
    e["temperature_dk"] = celsius * 10 + 2732
    return e


PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 40, "SEN3": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"cool": [entry(50)], "hot": [entry(56)]},
        "apct": [(2, [gddv.temp_cond("\\_SB_.SEN3", gddv.GREATER_OR_EQUAL, 59)]),
                 (1, [gddv.cond(gddv.DEFAULT)])],
        "apat": pl1max_target(1, "Cool", 35, psvt="cool") + pl1max_target(2, "Hot", 35, psvt="hot"),
    },
}


def pl1(g):
    return g.pl1("mmio")


def run(g):
    g.wait_ready()
    g.wait_until(lambda: pl1(g) == 35, 30, lambda: "SEN2 40 C: PL1 35 W (is %.1f W)" % pl1(g))
    g.set_temp("SEN2", 53)
    g.wait_until(lambda: pl1(g) == 10, 30, lambda: "SEN2 53 C: PL1 10 W (is %.1f W)" % pl1(g))

    g.set_temp("SEN3", 60)
    g.wait_log("Target Name:Hot")
    g.hold(lambda: pl1(g) <= 10.01, 12,
           lambda: "target Hot, SEN2 still 53 C: PL1 released to %.1f W" % pl1(g))
    g.check("target change: keep" in g.log(), "limit not carried over the target change")

    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 49)
    g.wait_until(lambda: pl1(g) == 35, 30, lambda: "SEN2 49 C: PL1 back at 35 W (is %.1f W)" % pl1(g))
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][n - 1:]
    d = [round(b - a, 3) for a, b in zip(w, w[1:]) if b != a]
    g.check(d and all(0 < x <= 5.01 for x in d), "release steps of at most 5 W expected: %s" % d)
    g.wait_log("carried over the target change released", timeout=10)
    g.check(g.stop_thermald() == 0, "exit status")
