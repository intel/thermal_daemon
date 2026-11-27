"""Two zones throttle the same cooling device (TCPU RAPL MMIO).

PSVT IETM.D0:
    SEN2 >= 50 C -> TCPU limit 20 W
    SEN3 >= 60 C -> TCPU limit MIN (PPCC minimum, 10 W)

  1. SEN2 hot:              PL1 settles at no more than 20 W
  2. SEN2 and SEN3 hot:     the stricter limit wins, PL1 near 10 W
  3. SEN3 cools, SEN2 hot:  PL1 stays limited (<= 20 W): one zone
                            releasing must not undo the other's limit
  4. both cool:             PL1 back at the PPCC maximum
"""

import gddv
from platforms import ppcc, rapl

PPCC_MIN_W, PPCC_MAX_W = 10, 35

PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 35, "SEN3": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(PPCC_MIN_W, PPCC_MAX_W)},
        "psvt": {"IETM.D0": [
            gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit=20000),
            gddv.psv(gddv.TCPU, "\\_SB_.SEN3", 60, limit="MIN"),
        ]},
    },
}


def pl1_msg(g, what):
    return lambda: "%s (MMIO PL1 is %.1f W)" % (what, g.pl1("mmio"))


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 20,
                 pl1_msg(g, "PL1 at the PPCC maximum"))

    g.set_temp("SEN2", 55)
    g.wait_until(lambda: g.pl1("mmio") <= 20, 60,
                 pl1_msg(g, "SEN2 hot: PL1 limited to 20 W"))

    g.set_temp("SEN3", 65)
    g.wait_until(lambda: g.pl1("mmio") <= PPCC_MIN_W + 2, 60,
                 pl1_msg(g, "SEN2+SEN3 hot: PL1 near the PPCC minimum"))

    g.set_temp("SEN3", 40)
    g.hold(lambda: g.pl1("mmio") <= 20, 15,
           pl1_msg(g, "SEN3 cooled while SEN2 is still hot: PL1 must stay "
                   "within SEN2's 20 W limit"))

    g.set_temp("SEN2", 40)
    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 60,
                 pl1_msg(g, "both cool: PL1 back at the PPCC maximum"))

    g.check(min(w["value"] for w in g.rapl_writes("mmio")) >= PPCC_MIN_W * 1e6,
            "PL1 written below the PPCC minimum")
