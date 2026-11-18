"""Motion conditions are supported: thermald treats the platform as
stationary (Motion == 0).

evaluate_condition() handled Motion, but verify_condition() rejected it
as unsupported, so a vault using it (e.g. a Dell Panther Lake laptop)
never had its condition sets used and always ran the highest-power
fallback target. "Moving" has the higher PL1MAX here, so the old
fallback picks it; the right answer is "Stationary".
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (2, [gddv.cond(gddv.MOTION, gddv.EQUAL, 1)]),
            (1, [gddv.cond(gddv.MOTION, gddv.EQUAL, 0)]),
        ],
        "apat": pl1max_target(1, "Stationary", 25) + pl1max_target(2, "Moving", 33),
    },
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("Unsupported condition 4" not in log, "Motion rejected as unsupported")
    g.check("Falling back to use configuration with the highest power" not in log,
            "fell back to the highest-power target")
    g.wait_until(lambda: g.pl1("mmio") == 25, 20,
                 lambda: "target Stationary: PL1 25 W (is %.1f W)" % g.pl1("mmio"))
    g.check("Target Name:Moving" not in g.log(), "target Moving selected")
