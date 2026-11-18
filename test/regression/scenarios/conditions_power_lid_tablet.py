"""APCT/APAT target selection from power source, lid and tablet mode.

Each target caps PL1 with PL1MAX, so the selected target is visible as
the MMIO PL1. Condition sets are checked in order, first match wins:

    lid closed      -> "LidClosed"  12 W
    tablet mode     -> "Tablet"     18 W
    on battery      -> "Battery"    20 W
    default         -> "AC"         30 W

Inputs: UPower (dbusmock), uinput lid and tablet-mode switches.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

TARGETS_W = {"AC": 30, "Battery": 20, "LidClosed": 12, "Tablet": 18}

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "switches": ["lid", "tablet"],
    "upower": {"on_battery": False},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (3, [gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
            (4, [gddv.cond(gddv.PLATFORM_TYPE, gddv.EQUAL, 2)]),
            (2, [gddv.cond(gddv.POWER_SOURCE, gddv.EQUAL, 1)]),
            (1, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": (pl1max_target(1, "AC", TARGETS_W["AC"])
                 + pl1max_target(2, "Battery", TARGETS_W["Battery"])
                 + pl1max_target(3, "LidClosed", TARGETS_W["LidClosed"])
                 + pl1max_target(4, "Tablet", TARGETS_W["Tablet"])),
    },
}


def expect_target(g, name, timeout=20):
    watts = TARGETS_W[name]
    g.wait_until(lambda: g.pl1("mmio") == watts, timeout,
                 lambda: "target %s: MMIO PL1 %d W (is %.1f W)"
                 % (name, watts, g.pl1("mmio")))
    g.wait_log("Target Name:%s" % name, timeout=5)


def run(g):
    g.wait_ready()
    g.check("Unsupported conditions" not in g.log(),
            "a condition was rejected at startup")

    expect_target(g, "AC")

    g.set_on_battery(True)
    expect_target(g, "Battery")

    g.set_lid_closed(True)          # lid wins over battery
    expect_target(g, "LidClosed")

    g.set_lid_closed(False)
    g.set_tablet_mode(True)         # tablet wins over battery
    expect_target(g, "Tablet")

    g.set_tablet_mode(False)
    expect_target(g, "Battery")

    g.set_on_battery(False)
    expect_target(g, "AC")
