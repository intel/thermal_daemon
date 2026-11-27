"""A PSVT entry with an absurd temperature is ignored, the rest works.

2147483000 dK is 214748027 C; in millicelsius that overflowed int in
install_passive() and silently became a trip point at -337.8 C, which
never fired. Now the entry is ignored with a warning, and the normal
SEN2 entry in the same PSVT still works.
"""

import gddv
from platforms import ppcc, rapl

PLATFORM = {
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 35, "SEN3": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [
            gddv.psv(gddv.TCPU, "\\_SB_.SEN3", None, temperature_dk=2147483000),
            gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50),
        ]},
    },
}


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == 35, 20,
                 lambda: "PL1 at the PPCC maximum (is %.1f W)" % g.pl1("mmio"))
    g.hold(lambda: g.pl1("mmio") == 35, 8,
           lambda: "PL1 throttled with every zone at 35 C (%.1f W)"
           % g.pl1("mmio"))
    g.check("temperature 2147483000 dK out of range, ignored" in g.log(),
            "the out-of-range PSVT temperature was not reported")

    g.set_temp("SEN2", 60)
    g.wait_until(lambda: g.pl1("mmio") <= 15, 60,
                 lambda: "SEN2 hot: PL1 throttled (is %.1f W)" % g.pl1("mmio"))
