"""Two GDDV virtual sensors, VR1.D0 and VR2.D0 (6656673, 5ada502).

Like the Samsung Galaxy Book5 Pro 360: the PSVT only targets VR1. Each
virtual sensor must get its own sensor and zone; before the fix both
were merged into one named after the last one parsed (VR2), so there was
no zone for VR1, thermald wrote ignore_adaptive and exited.

VR1 = SEN3, VR2 = SEN4 (coeff 1.0, no smoothing). PSVT: throttle TCPU
when VR1 >= 50 C. Heating SEN4 alone must not throttle.
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN3", "SEN4"],
    # thermald's zone for a virtual sensor binds intel_powerclamp, which
    # the VM doesn't have (no MWAIT)
    "cdevs": ["intel_powerclamp"],
    "initial_temps": {"SEN3": 35, "SEN4": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "vsct": {
            "VR1.D0": [("\\_SB_.SEN3", 1000, 1000)],
            "VR2.D0": [("\\_SB_.SEN4", 1000, 1000)],
        },
        "vspt": {
            "VR1.D0": [(0, 1)],
            "VR2.D0": [(0, 1)],
        },
        "psvt": {"IETM.D0": [
            gddv.psv(gddv.TCPU, "\\_SB_.VR1", 50, limit="MIN"),
        ]},
    },
}


def run(g):
    g.wait_ready()
    log = g.log()
    for name in ("VR1.D0", "VR2.D0"):
        g.check("Found virtual sensor [%s]" % name in log,
                "no virtual sensor for %s" % name)
    g.check("Unable to find a zone for VR1" not in log,
            "PSVT target VR1 has no zone")
    g.wait_until(lambda: g.pl1("mmio") == 35, 20,
                 lambda: "MMIO PL1 at the PPCC maximum (is %.1f W)"
                 % g.pl1("mmio"))

    g.set_temp("SEN4", 60)          # VR2 only: no PSVT entry
    g.hold(lambda: g.pl1("mmio") == 35, 8,
           lambda: "VR2 hot throttled TCPU (PL1 %.1f W)" % g.pl1("mmio"))

    g.set_temp("SEN3", 60)          # VR1 >= 50 C
    g.wait_until(lambda: g.pl1("mmio") <= 15, 60,
                 lambda: "VR1 hot: TCPU throttled (PL1 %.1f W)"
                 % g.pl1("mmio"))

    g.set_temp("SEN3", 40)
    g.wait_until(lambda: g.pl1("mmio") == 35, 60,
                 lambda: "VR1 cool: PL1 restored (is %.1f W)" % g.pl1("mmio"))
