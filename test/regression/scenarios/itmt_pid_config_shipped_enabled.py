"""The itmt-pid.xml that thermald installs (data/itmt-pid.xml), with
Enable changed to 1, is read without warnings and enables the PID with the
built-in gains.

Same ITMT entry as itmt_pid_config (10 W PL1 range): Kp 30 and Kd 1000
scaled by 10 W / 17 W.
"""

import os

import gddv
from platforms import ppcc, rapl

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "..", "..", "data", "itmt-pid.xml")) as f:
    CONFIG = f.read()
assert CONFIG.count("<Enable>0</Enable>") == 1
CONFIG = CONFIG.replace("<Enable>0</Enable>", "<Enable>1</Enable>")

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
    g.check("itmt-pid.xml: ITMT PID enabled, Kp 30 Kd 1000 set point offset 500 scale with "
            "range (reference 17000 mW, 0.5-1.5)" in log,
            "the shipped itmt-pid.xml, enabled, doesn't give the built-in defaults")
    warnings = [l for l in log.splitlines() if "itmt-pid.xml" in l and "[WARN]" in l]
    g.check(not warnings, "warnings for the shipped itmt-pid.xml: %s" % warnings)
    g.check("pid valid 17.647059:0.000000:588.235294" in log,
            "Kp 30 / Kd 1000 scaled by 10/17 expected")
    g.check(g.stop_thermald() == 0, "exit status")
