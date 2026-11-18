"""ITMT v1 whose last entry ends right after pl1_max (d0929bb).

Seen on an ASUS Vivobook X1402ZA (thermald issue #463). The short entry
must not abort GDDV parsing: the PPCC and PSVT in the same vault are
used and PL1 goes to the PPCC maximum.
"""

import gddv
from platforms import base_vault, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": base_vault(raw={
        "/shared/export/itmt": gddv.itmt_v1(
            [("\\_SB_.PC00.LPCB.EC0_.SEN1", 62, 28000, 35000)],
            short_last=True),
    }),
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("out of bounds" not in log and "expecting 8" not in log,
            "GDDV parsing failed on the short ITMT entry")
    g.check("IETM_D0 processed" in log, "IETM.D0 not installed")
    g.wait_until(lambda: g.pl1("mmio") == 35, 20,
                 lambda: "MMIO PL1 at the PPCC maximum (is %.1f W)"
                 % g.pl1("mmio"))
