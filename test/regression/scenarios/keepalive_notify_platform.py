"""A keep-alive request doesn't overwrite an active NotifyPlatform value.

The AC target has NotifyPlatform 5 on IETM, the Battery target none. A
keep-alive request (KALV: Notify(IETM, 0xA0)) is answered through
IETM.IMOK with the active NotifyPlatform value, else with 1:

  1. AC: IMOK(5) for the target, then IMOK(5) again for a keep-alive.
  2. Battery: a keep-alive is answered with IMOK(1).
"""

import time

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

KALV = "\\_SB.IETM.KALV"

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "upower": {"on_battery": False},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (2, [gddv.cond(gddv.POWER_SOURCE, gddv.EQUAL, 1)]),
            (1, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": (pl1max_target(1, "AC", 30)
                 + [gddv.action(1, "AC", gddv.IETM, "NotifyPlatform", 5)]
                 + pl1max_target(2, "Battery", 20)),
    },
}


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.imok_values() == [5], 20,
                 lambda: "IMOK(5) for target AC, got %s" % g.imok_values())
    g.acpi_eval(KALV)
    g.wait_until(lambda: g.imok_values() == [5, 5], 10,
                 lambda: "keep-alive answered with IMOK(5) on AC, got %s"
                 % g.imok_values())

    g.set_on_battery(True)
    g.wait_log("set_int3400 target", 20)
    time.sleep(3)
    before = list(g.imok_values())
    g.acpi_eval(KALV)
    g.wait_until(lambda: g.imok_values() == before + [1], 10,
                 lambda: "keep-alive answered with IMOK(1) on battery, got %s"
                 % g.imok_values())
    g.check(g.stop_thermald() == 0, "exit status")
