"""ITMT: a "MIN" limit and an entry naming B0D4 on a TCPU platform.

The ITMT table names the processor \\_SB_.PC00.B0D4, as some vaults do,
but the platform's processor zone is TCPU: thermald must use the TCPU
zone (install_itmt() tries B0D4 <-> TCPU). The limit above 65 C is
"MIN": PL1 goes to the PPCC minimum, 10 W, not to 0.
"""

import gddv
from platforms import ppcc, rapl

B0D4 = "\\_SB_.PC00.B0D4"

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(B0D4, 65, "MIN", "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    g.check("Unable to find a zone for" not in g.log(),
            "the B0D4 entry found no zone")
    g.wait_until(lambda: g.pl1("mmio") == 35, 30,
                 lambda: "TCPU 45 C: PL1 35 W (is %.1f W)" % g.pl1("mmio"))

    g.set_temp("TCPU", 75)
    g.wait_until(lambda: g.pl1("mmio") == 10, 90,
                 lambda: "TCPU 75 C: PL1 at the PPCC minimum 10 W (is %.1f W)"
                 % g.pl1("mmio"))
    writes = [w["value"] / 1e6 for w in g.rapl_writes("mmio")]
    g.check(min(writes) >= 10, "PL1 written below the PPCC minimum: %s" % writes)

    g.set_temp("TCPU", 45)
    g.wait_until(lambda: g.pl1("mmio") == 35, 60,
                 lambda: "TCPU 45 C again: PL1 35 W (is %.1f W)" % g.pl1("mmio"))
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
