"""A virtual sensor keeps millidegree precision.

VR1 = 0.5 x SEN3 (no smoothing). A condition VR1 >= 30.8 C selects
target "Hot", otherwise "Normal". With SEN3 at 61 C, VR1 is 30.5 C:
thermald used to round the result to whole degrees (31 C) before
converting to millicelsius, so "Hot" was selected. At 62 C (VR1 31.0 C)
it must be.

The same rounding made a power based virtual sensor (in W) jump from
2.5 to 3 W on a real data vault.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 35, "SEN3": 40},
    "cdevs": ["intel_powerclamp"],      # virtual sensor zones need it
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt("SEN2")},
        "vsct": {"VR1.D0": [("\\_SB_.SEN3", 500, 1000)]},
        "vspt": {"VR1.D0": [(0, 1)]},
        "apct": [
            # 30.8 C in deci-Kelvin
            (2, [gddv.cond(gddv.TEMPERATURE, gddv.GREATER_OR_EQUAL,
                           308 + 2732, "\\_SB_.VR1")]),
            (1, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": pl1max_target(1, "Normal", 30) + pl1max_target(2, "Hot", 18),
    },
}


def run(g):
    g.wait_ready()
    g.check("Found virtual sensor [VR1.D0]" in g.log(), "VR1 not installed")
    g.wait_until(lambda: g.pl1("mmio") == 30, 20,
                 lambda: "target Normal: PL1 30 W (is %.1f W)" % g.pl1("mmio"))

    g.set_temp("SEN3", 61)                  # VR1 = 30.5 C < 30.8 C
    g.hold(lambda: g.pl1("mmio") == 30, 8,
           lambda: "VR1 30.5 C selected Hot (PL1 %.1f W): virtual sensor "
           "value rounded to whole degrees" % g.pl1("mmio"))

    g.set_temp("SEN3", 62)                  # VR1 = 31.0 C
    g.wait_until(lambda: g.pl1("mmio") == 18, 20,
                 lambda: "VR1 31.0 C: target Hot, PL1 18 W (is %.1f W)"
                 % g.pl1("mmio"))
