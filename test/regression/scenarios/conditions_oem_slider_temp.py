"""APCT/APAT target selection from OEM variables, the power slider and a
temperature condition, including a set of two ANDed conditions.

ODVP gives odvp0 = 5. Condition sets, first match wins:

    Oem0 == 7                       -> "OemMiss"   11 W (never)
    Oem0 == 5 AND slider == 25      -> "SaverOem"  14 W
    SEN2 >= 50 C                    -> "Hot"       16 W
    slider == 100                   -> "Perf"      33 W
    default                         -> "Balanced"  25 W

Inputs: ODVP in the IETM SSDT, power-profiles-daemon (dbusmock).
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

TARGETS_W = {"OemMiss": 11, "SaverOem": 14, "Hot": 16, "Perf": 33,
             "Balanced": 25}

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "odvp": [5, 0],
    "power_profile": "balanced",
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (1, [gddv.cond(gddv.OEM0, gddv.EQUAL, 7)]),
            (2, [gddv.cond(gddv.OEM0, gddv.EQUAL, 5),
                 gddv.cond(gddv.POWER_SLIDER, gddv.EQUAL, 25)]),
            (3, [gddv.temp_cond("\\_SB_.SEN2", gddv.GREATER_OR_EQUAL, 50)]),
            (4, [gddv.cond(gddv.POWER_SLIDER, gddv.EQUAL, 100)]),
            (5, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": (pl1max_target(1, "OemMiss", TARGETS_W["OemMiss"])
                 + pl1max_target(2, "SaverOem", TARGETS_W["SaverOem"])
                 + pl1max_target(3, "Hot", TARGETS_W["Hot"])
                 + pl1max_target(4, "Perf", TARGETS_W["Perf"])
                 + pl1max_target(5, "Balanced", TARGETS_W["Balanced"])),
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

    expect_target(g, "Balanced")

    g.set_power_profile("performance")
    expect_target(g, "Perf")

    g.set_temp("SEN2", 55)              # temperature set comes before slider
    expect_target(g, "Hot")

    g.set_temp("SEN2", 40)
    expect_target(g, "Perf")

    g.set_power_profile("power-saver")  # both ANDed conditions now true
    expect_target(g, "SaverOem")

    g.set_power_profile("balanced")     # only Oem0 still true
    expect_target(g, "Balanced")

    g.check("Target Name:OemMiss" not in g.log(),
            "Oem0 == 7 matched although odvp0 is 5")
