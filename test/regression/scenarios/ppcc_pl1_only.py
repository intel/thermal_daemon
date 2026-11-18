"""A PPCC with only a PL1 entry (84 bytes) must still be used.

Same PPCC limits as psvt_throttle_restore (PL1 10-35 W), but without the
PL2 block. thermald must set PL1 to the PPCC maximum at start and
throttle within the PPCC range.

parse_ppcc() used to return before storing a PPCC shorter than 156 bytes
(no PL2 block), so RAPL fell back to limits derived from max_power_uw.
"""

import gddv
from platforms import ppcc, rapl

PPCC_MIN_W, PPCC_MAX_W = 10, 35

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc_nr_limits": 1,
        "ppcc": {"TCPU.D0": ppcc(PPCC_MIN_W, PPCC_MAX_W)},
        "psvt": {"IETM.D0": [
            gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit="MIN"),
        ]},
    },
}


def run(g):
    g.wait_ready()
    g.check("ppcc limits max:%d" % (PPCC_MAX_W * 1000000) in g.log(),
            "the PL1-only PPCC was not used for the RAPL device")
    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 20,
                 lambda: "MMIO PL1 at the PPCC maximum %d W (is %.1f W)"
                 % (PPCC_MAX_W, g.pl1("mmio")))

    g.set_temp("SEN2", 60)
    g.wait_until(lambda: g.pl1("mmio") <= PPCC_MIN_W + 5, 90,
                 lambda: "MMIO PL1 throttled to near the PPCC minimum "
                 "(is %.1f W)" % g.pl1("mmio"))
    g.check(min(w["value"] for w in g.rapl_writes("mmio")) >= PPCC_MIN_W * 1e6,
            "PL1 written below the PPCC minimum")
