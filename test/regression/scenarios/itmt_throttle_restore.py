"""ITMT: PL1 follows the ITMT table as the CPU temperature crosses its
trips, and is restored on exit.

The vault's only target is an IETM ITMT action ("IETM.D0"), selected by
a default condition; the IDSP lists the ITMT UUID, which thermald needs
before it uses the table. ITMT v1, both entries on TCPU, PL1 unlimited
("MAX") below the trip:

    above 60 C   PL1 25 W
    above 70 C   PL1 15 W

PPCC: PL1 10-35 W.

  1. TCPU 45 C: PL1 at the PPCC maximum, 35 W.
  2. TCPU 65 C: PL1 comes down to 25 W.
  3. TCPU 75 C: PL1 comes down to 15 W.
  4. TCPU 65 C: only the 60 C trip is active, PL1 back up to 25 W
     (not the 35 W maximum).
  5. TCPU 45 C: PL1 back at 35 W.
  6. SIGTERM: the power-on PL1 is restored.

(The test encoder writes 60 C as 59.5 C, see gddv.celsius_to_dk().)
"""

import gddv
from platforms import POWER_ON_PL1_W, ppcc, rapl

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 25000, "MAX"),
                             (gddv.TCPU, 70, 15000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def expect_pl1(g, watts, what, timeout=60):
    g.wait_until(lambda: g.pl1("mmio") == watts, timeout,
                 lambda: "%s: MMIO PL1 %g W (is %.1f W)"
                 % (what, watts, g.pl1("mmio")))


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    g.check("no IDSP support" not in g.log(), "ITMT table not used")
    expect_pl1(g, 35, "TCPU 45 C, no trip")

    g.set_temp("TCPU", 65)
    expect_pl1(g, 25, "TCPU 65 C, above the 60 C trip")
    g.set_temp("TCPU", 75)
    expect_pl1(g, 15, "TCPU 75 C, above the 70 C trip")
    writes = [w["value"] / 1e6 for w in g.rapl_writes("mmio")]
    g.check(min(writes) >= 15, "PL1 written below the ITMT limit: %s" % writes)

    g.set_temp("TCPU", 65)
    expect_pl1(g, 25, "TCPU back at 65 C, only the 60 C trip active")
    g.set_temp("TCPU", 45)
    expect_pl1(g, 35, "TCPU 45 C again")

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
