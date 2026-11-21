"""A Temperature condition has a 2 C hysteresis: once matched, "SEN2 >= 59 C"
stays matched until SEN2 is below 57 C, so a temperature near the argument
doesn't switch the target back and forth.

Condition sets, first match wins:

    SEN2 >= 59 C    -> "Hot"   PL1 16 W
    default         -> "Cool"  PL1 25 W

  1. SEN2 35 C: Cool.
  2. SEN2 59 C: Hot.
  3. SEN2 58 C, then 57.5 C: still Hot.
  4. SEN2 56.5 C: Cool.
  5. SEN2 58 C, not yet 59 C: still Cool.
"""

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

TARGETS_W = {"Hot": 16, "Cool": 25}

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [
            (1, [gddv.temp_cond("\\_SB_.SEN2", gddv.GREATER_OR_EQUAL, 59)]),
            (2, [gddv.cond(gddv.DEFAULT)]),
        ],
        "apat": (pl1max_target(1, "Hot", TARGETS_W["Hot"])
                 + pl1max_target(2, "Cool", TARGETS_W["Cool"])),
    },
}


def pl1(g):
    return g.pl1("mmio")


def expect_target(g, name, timeout=20):
    g.wait_until(lambda: pl1(g) == TARGETS_W[name], timeout,
                 lambda: "target %s: MMIO PL1 %d W (is %.1f W)"
                 % (name, TARGETS_W[name], pl1(g)))


def stays(g, name, what):
    g.hold(lambda: pl1(g) == TARGETS_W[name], 10,
           lambda: "%s: target %s left (MMIO PL1 %.1f W)" % (what, name, pl1(g)))


def run(g):
    g.wait_ready()
    expect_target(g, "Cool")

    g.set_temp("SEN2", 59)
    expect_target(g, "Hot")

    g.set_temp("SEN2", 58)
    stays(g, "Hot", "SEN2 58 C, within the hysteresis")
    g.set_temp("SEN2", 57.5)
    stays(g, "Hot", "SEN2 57.5 C, within the hysteresis")

    g.set_temp("SEN2", 56.5)
    expect_target(g, "Cool")

    g.set_temp("SEN2", 58)
    stays(g, "Cool", "SEN2 58 C, below the condition")

    g.check(g.stop_thermald() == 0, "exit status")
