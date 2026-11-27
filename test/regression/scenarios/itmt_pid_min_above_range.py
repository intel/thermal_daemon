"""ITMT PID (Enable in itmt-pid.xml) with an ITMT entry whose PL1 minimum
is above the PPCC maximum (as on a Chuwi UniBook: "PL1 28000-MAX" with
PL1 15-20 W): the entry doesn't limit, as with the default controller.

  1. TCPU 45 C: PL1 at the PPCC maximum, 20 W.
  2. TCPU 70 C, above the 60 C trip, for 30 s: PL1 stays at 20 W.
"""

import gddv
from platforms import ITMT_PID_ON, ppcc, rapl

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "thermald_args": ["--adaptive"],
    "conf_files": {"itmt-pid.xml": ITMT_PID_ON},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(15, 20)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 28000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    g.wait_until(lambda: g.pl1("mmio") == 20, 30,
                 lambda: "TCPU 45 C: PL1 20 W (is %.1f W)" % g.pl1("mmio"))
    g.set_temp("TCPU", 70)
    g.hold(lambda: g.pl1("mmio") == 20, 30,
           lambda: "TCPU 70 C: PL1 limited to %.1f W by an entry whose PL1 "
           "minimum is above the PL1 range" % g.pl1("mmio"))
    g.check(g.stop_thermald() == 0, "exit status")
