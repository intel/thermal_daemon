"""With RAPL MMIO present, thermald controls MMIO and parks the MSR
interface out of the way, then restores both on exit.

  - MMIO PL1 at the PPCC maximum (35 W), MSR PL1 raised to 200 W, so the
    MMIO limit is the effective one: the package (45 W demand) draws 35 W
  - SIGTERM: both domains back at the power-on PL1 (15 W)
"""

import time

from platforms import POWER_ON_PL1_W, idle_psvt, ppcc, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(demand_w=45),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": idle_psvt()},
    },
}


def package_power_w(g, seconds=2.0):
    path = "/sys/devices/virtual/powercap/intel-rapl-mmio/intel-rapl-mmio:0/energy_uj"
    e1 = int(open(path).read())
    time.sleep(seconds)
    e2 = int(open(path).read())
    return (e2 - e1) / seconds / 1e6


def run(g):
    g.wait_ready()
    g.wait_log("Disable rapl-msr interface and use rapl-mmio")
    g.wait_until(lambda: g.pl1("mmio") == 35 and g.pl1("msr") == 200, 20,
                 lambda: "MMIO PL1 35 W and MSR PL1 parked at 200 W "
                 "(are %.1f / %.1f W)" % (g.pl1("mmio"), g.pl1("msr")))

    power = package_power_w(g)
    g.check(33 <= power <= 37,
            "package power %.1f W, expected the 35 W MMIO limit" % power)

    status = g.stop_thermald()
    g.check(status == 0, "thermald exit status %s" % status)
    for d in ("mmio", "msr"):
        g.check(g.pl1(d) == POWER_ON_PL1_W,
                "%s PL1 after exit %.1f W, expected power-on %d W"
                % (d, g.pl1(d), POWER_ON_PL1_W))
