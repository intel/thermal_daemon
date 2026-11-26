"""The itmt-pid.xml that thermald installs (data/itmt-pid.xml) is read
without warnings, gives the built-in gains and leaves the PID disabled:
the ITMT trip uses the default controller.

Same ITMT entry as itmt_pid_config. TCPU 65 C: PL1 still comes down to
the entry's 25 W.
"""

import os

import gddv
from platforms import ppcc, rapl

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "..", "..", "data", "itmt-pid.xml")) as f:
    CONFIG = f.read()

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "thermald_args": ["--adaptive"],
    "conf_files": {"itmt-pid.xml": CONFIG},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 25000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    log = g.log()
    g.check("itmt-pid.xml: ITMT PID disabled, Kp 30 Kd 1000 set point offset 500 "
            "scale with range (reference 17000 mW, 0.5-1.5)" in log,
            "the shipped itmt-pid.xml doesn't give the built-in defaults, disabled")
    warnings = [l for l in log.splitlines() if "itmt-pid.xml" in l and "[WARN]" in l]
    g.check(not warnings, "warnings for the shipped itmt-pid.xml: %s" % warnings)
    g.check("ITMT PID" not in log.replace("itmt-pid.xml: ITMT PID", ""),
            "ITMT PID used although the shipped itmt-pid.xml disables it")
    g.check("pid valid" not in log, "a trip has a PID")

    g.set_temp("TCPU", 65)
    g.wait_until(lambda: g.pl1("mmio") == 25, 60,
                 lambda: "TCPU 65 C: PL1 down to 25 W (is %.1f W)" % g.pl1("mmio"))
    g.check(g.stop_thermald() == 0, "exit status")
