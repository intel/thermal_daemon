"""No condition set can match: fall back to the highest-power target.

There is no UPower and no lid switch, so the Power_source and Lid_state
conditions are unsupported, and Oem0 == 9 is false (odvp0 = 0). thermald
must keep running and apply the target with the highest PL1MAX ("Turbo").

thermald used to pick the fallback target but never apply it:
update_engine_state() returned on "No change in target"
(current_matched_target and target both -1) before reaching
exec_fallback_target(), since e39a74a.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "odvp": [0],
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (1, [gddv.cond(gddv.POWER_SOURCE, gddv.EQUAL, 1)]),
            (2, [gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
            (3, [gddv.cond(gddv.OEM0, gddv.EQUAL, 9)]),
        ],
        "apat": (pl1max_target(1, "Battery", 20)
                 + pl1max_target(2, "LidClosed", 12)
                 + pl1max_target(3, "Turbo", 32)),
    },
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("Some conditions are not supported" in log,
            "unsupported Power_source/Lid_state conditions not reported")
    g.check("Falling back to use configuration with the highest power" in log,
            "no fallback to the highest-power target")

    g.wait_until(lambda: g.pl1("mmio") == 32, 20,
                 lambda: "fallback target Turbo: MMIO PL1 32 W (is %.1f W)"
                 % g.pl1("mmio"))

    # Stays there, and thermald keeps running
    g.wait_until(lambda: g.now() > 20, 30, "20 s of running")
    g.check(g.thermald_running(), "thermald exited")
    g.check(g.pl1("mmio") == 32, "MMIO PL1 moved off the fallback target: "
            "%.1f W" % g.pl1("mmio"))
