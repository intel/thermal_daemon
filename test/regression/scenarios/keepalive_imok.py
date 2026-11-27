"""Keep-alive requests of the firmware are answered through IETM.IMOK.

The SSDT's KALV method does Notify(IETM, 0xA0), as firmware does to check
that a thermal daemon runs; int3400_thermal turns it into a thermal zone
uevent with NAME=INT3400 Thermal and EVENT=8 (THERMAL_EVENT_KEEP_ALIVE).
The SSDT's IMOK logs its argument to dmesg.

  1. While thermald runs, each keep-alive request is answered once with
     IMOK(1), within a few seconds.
  2. After thermald exited, a request is not answered (the firmware's
     failsafe can engage).
"""

import time

import gddv
from platforms import idle_psvt, pl1max_target, ppcc, rapl

KALV = "\\_SB.IETM.KALV"

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc()},
        "psvt": {"IETM.D0": idle_psvt()},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": pl1max_target(1, "Default", 30),
    },
}


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 target")
    time.sleep(2)
    g.check(g.imok_values() == [], "IMOK called before any keep-alive: %s"
            % g.imok_values())

    for n in (1, 2, 3):
        g.acpi_eval(KALV)
        g.wait_until(lambda: g.imok_values() == [1] * n, 10,
                     lambda: "IMOK(1) for keep-alive %d, got %s"
                     % (n, g.imok_values()))
        time.sleep(1)

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.acpi_eval(KALV)
    time.sleep(5)
    g.check(g.imok_values() == [1, 1, 1],
            "keep-alive answered after thermald exited: %s" % g.imok_values())
