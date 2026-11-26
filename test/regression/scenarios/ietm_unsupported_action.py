"""An IETM action thermald doesn't handle (EPOT) must not change the
trips another action of the same target installed.

The target installs a PSVT with a single MAX entry (SEN2 >= 39 C), for
which thermald adds its default trip 1 C higher, and then has an EPOT
action, as on a ThinkPad data vault. EPOT used to run the PSVT
consolidation again, which moved the 39 C trip to 40 C.
"""

import re

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        # No step size: the trips of the PSVT consolidation, not step control
        "psvt": {"t0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 39, limit="MAX",
                                 step_size=0)]},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [
            gddv.action(1, "T0", gddv.TCPU, "PL1MAX", 30000),
            gddv.action(1, "T0", gddv.IETM, "PSVT", "t0"),
            gddv.action(1, "T0", gddv.IETM, "EPOT", "ac_g2_dc_g5"),
        ],
    },
}


def sen2_passive_trips(log):
    dump = log[log.rfind("ZONE DUMP BEGIN"):]
    dump = dump[:dump.find("ZONE DUMP END")]
    zone = dump[dump.find(": SEN2,"):]
    return [int(t) // 1000 for t in re.findall(r"type:passive temp:(\d+)", zone)]


def run(g):
    g.wait_ready()
    # All three actions of T0 executed (each logs its target name)
    g.wait_until(lambda: g.log().count("Target Name:T0") >= 3, 20,
                 "the three actions of target T0")
    g.hold(lambda: True, 3, "")
    log = g.log()
    trips = sen2_passive_trips(log)
    g.check(trips == [39, 40], "SEN2 passive trips %s, expected [39, 40] "
            "(the PSVT entry and thermald's +1 C trip)" % trips)
    g.check("Unsupported IETM target code EPOT" in log,
            "EPOT not reported as unsupported")
