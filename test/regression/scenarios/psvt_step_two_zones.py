"""PSVT step control with two sensors, in two zones, limiting the same PL1:
the most restrictive request applies; when that sensor cools down, the
other one's limit applies again, not the PL1 maximum.

PPCC: PL1 10-35 W. PSVT entries, step 500 mW, coefficients 10/10
(5 W per 1 s period): SEN2 50 C -> 20 W, SEN3 50 C -> 15 W.

  1. Both 40 C: PL1 35 W.
  2. SEN2 55 C: 20 W.          3. SEN3 55 C as well: 15 W.
  4. SEN3 40 C: back to SEN2's 20 W, held there.
  5. SEN2 40 C: 35 W.
  6. SEN3 55 C, then SEN2 55 C: 15 W stays (the stricter one first).
"""

import gddv
from platforms import ppcc, rapl


def entry(sensor, limit):
    e = gddv.psv(gddv.TCPU, "\\_SB_." + sensor, 50, limit=limit, sample_period=10,
                 step_size=500, limit_coeff=10, unlimit_coeff=10)
    e["temperature_dk"] = 50 * 10 + 2732
    return e


PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 40, "SEN3": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [entry("SEN2", 20000), entry("SEN3", 15000)]},
    },
}


def expect(g, watts, what, timeout=30):
    g.wait_until(lambda: abs(g.pl1("mmio") - watts) < 0.01, timeout,
                 lambda: "%s: PL1 %g W (is %.2f W)" % (what, watts, g.pl1("mmio")))


def run(g):
    g.wait_ready()
    expect(g, 35, "both cool")
    g.set_temp("SEN2", 55)
    expect(g, 20, "SEN2 hot")
    g.set_temp("SEN3", 55)
    expect(g, 15, "SEN2 and SEN3 hot: the most restrictive")
    g.set_temp("SEN3", 40)
    expect(g, 20, "SEN3 cool again: SEN2's limit")
    g.hold(lambda: abs(g.pl1("mmio") - 20) < 0.01, 10,
           lambda: "SEN2 still hot: PL1 left 20 W (%.2f W)" % g.pl1("mmio"))
    g.set_temp("SEN2", 40)
    expect(g, 35, "both cool again")
    g.set_temp("SEN3", 55)
    expect(g, 15, "SEN3 hot")
    g.set_temp("SEN2", 55)
    g.hold(lambda: abs(g.pl1("mmio") - 15) < 0.01, 10,
           lambda: "SEN2 hot after SEN3: PL1 left 15 W (%.2f W)" % g.pl1("mmio"))
    g.check(g.stop_thermald() == 0, "exit status")
