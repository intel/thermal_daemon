"""NotifyPlatform targets reach the firmware through IETM.IMOK
(4160d88, 30cdfca).

The SSDT's IMOK logs its argument to dmesg. Targets:
    AC:      NotifyPlatform 5 on IETM, and NotifyPlatform 9 on TCPU
    Battery: NotifyPlatform 7 on IETM
Only IETM NotifyPlatform targets may call IMOK: 9 must never appear.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

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
                 + [gddv.action(1, "AC", gddv.IETM, "NotifyPlatform", 5),
                    gddv.action(1, "AC", gddv.TCPU, "NotifyPlatform", 9)]
                 + pl1max_target(2, "Battery", 20)
                 + [gddv.action(2, "Battery", gddv.IETM, "NotifyPlatform", 7)]),
    },
}


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.imok_values() == [5], 20,
                 lambda: "IMOK(5) for target AC, got %s" % g.imok_values())

    g.set_on_battery(True)
    g.wait_until(lambda: g.imok_values() == [5, 7], 20,
                 lambda: "IMOK(7) for target Battery, got %s"
                 % g.imok_values())

    g.set_on_battery(False)
    g.wait_until(lambda: g.imok_values() == [5, 7, 5], 20,
                 lambda: "IMOK(5) again for target AC, got %s"
                 % g.imok_values())

    g.check(9 not in g.imok_values(),
            "NotifyPlatform on TCPU reached IETM.IMOK")
