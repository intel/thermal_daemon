"""A lid switch input device disappears while a lid condition is in use
(PR #611: dc3acfc, c566a7d, 8eeddcf).

Before the fix, libevdev kept reporting pending events on the removed
device while reads failed with -ENODEV, and thermald spun at 100% CPU.
Now the device is released, the lid counts as open, and CPU use stays
low.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "switches": ["lid"],
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (2, [gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
            (1, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": pl1max_target(1, "Open", 30) + pl1max_target(2, "Closed", 12),
    },
}


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == 30, 20,
                 lambda: "lid open: PL1 30 W (is %.1f W)" % g.pl1("mmio"))

    g.set_lid_closed(True)
    g.wait_until(lambda: g.pl1("mmio") == 12, 20,
                 lambda: "lid closed: PL1 12 W (is %.1f W)" % g.pl1("mmio"))

    g.remove_switch("lid")
    before = g.cpu_seconds()
    g.hold(lambda: True, 6, "")
    used = g.cpu_seconds() - before
    g.check(used < 0.6, "thermald used %.2f s of CPU in 6 s after the "
            "lid switch went away (busy loop?)" % used)

    g.wait_log("is gone, no longer using it", timeout=15)
    g.wait_until(lambda: g.pl1("mmio") == 30, 20,
                 lambda: "lid switch gone counts as open: PL1 30 W "
                 "(is %.1f W)" % g.pl1("mmio"))
