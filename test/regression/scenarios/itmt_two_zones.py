"""ITMT with two sensors, in two zones, limiting the same PL1: the
most restrictive entry applies; when that sensor cools down, the other
one's limit applies again, not the PL1 maximum.

PPCC: PL1 10-35 W. ITMT entries (60 C, written as 59.5 C):
SEN2 -> PL1 down to 25 W, SEN3 -> down to 15 W, no upper limit.

  1. Both 45 C: PL1 35 W.
  2. SEN2 65 C: 25 W.          3. SEN3 65 C as well: 15 W.
  4. SEN3 45 C: back to SEN2's 25 W, held there.
  5. SEN2 45 C: 35 W.
  6. SEN3 65 C, then SEN2 65 C: 15 W stays (the stricter one first).
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 45, "SEN3": 45},
    "rapl": rapl(),
    "thermald_args": ["--adaptive"],
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [("\\_SB_.SEN2", 60, 25000, "MAX"),
                             ("\\_SB_.SEN3", 60, 15000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def expect(g, watts, what, timeout=60):
    g.wait_until(lambda: abs(g.pl1("mmio") - watts) < 0.01, timeout,
                 lambda: "%s: PL1 %g W (is %.2f W)" % (what, watts, g.pl1("mmio")))


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    expect(g, 35, "both cool")
    g.set_temp("SEN2", 65)
    expect(g, 25, "SEN2 hot")
    g.set_temp("SEN3", 65)
    expect(g, 15, "SEN2 and SEN3 hot: the most restrictive")
    writes = [w["value"] / 1e6 for w in g.rapl_writes("mmio")]
    g.check(min(writes) >= 15 - 0.01, "PL1 written below 15 W: %s" % writes)
    g.set_temp("SEN3", 45)
    expect(g, 25, "SEN3 cool again: SEN2's limit")
    g.hold(lambda: abs(g.pl1("mmio") - 25) < 0.01, 10,
           lambda: "SEN2 still hot: PL1 left 25 W (%.2f W)" % g.pl1("mmio"))
    g.set_temp("SEN2", 45)
    expect(g, 35, "both cool again")
    g.set_temp("SEN3", 65)
    expect(g, 15, "SEN3 hot")
    g.set_temp("SEN2", 65)
    g.hold(lambda: abs(g.pl1("mmio") - 15) < 0.01, 10,
           lambda: "SEN2 hot after SEN3: PL1 left 15 W (%.2f W)" % g.pl1("mmio"))
    g.check(g.stop_thermald() == 0, "exit status")
