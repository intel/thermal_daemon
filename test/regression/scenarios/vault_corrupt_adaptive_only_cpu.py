"""Corrupt vault on a CPU that thermald supports only in adaptive mode
(Panther Lake L, 6:0xcc).

Adaptive mode fails and the default engine refuses this CPU, so thermald
must exit with EXIT_UNSUPPORTED (2), without crashing, after saying why.
"""

import gddv
from platforms import rapl

PLATFORM = {
    "cpu_model": 0xcc,
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {"raw_vault": gddv.HEADER + b"\xff" * 64},
}


def run(g):
    g.wait_until(lambda: not g.thermald_running(), 40,
                 "thermald to give up on this platform")
    log = g.log()
    g.check(g.proc.returncode == 2, "exit status %s, expected 2 "
            "(EXIT_UNSUPPORTED)" % g.proc.returncode)
    g.check("GDDV parsing failed" in log or "Unable to parse GDDV" in log,
            "vault error not reported")
    g.check("supported only in adaptive performance mode" in log,
            "adaptive-only CPU not reported")
    for bad in ("Segmentation", "terminate called", "Aborted"):
        g.check(bad not in log, "thermald log contains %r" % bad)
