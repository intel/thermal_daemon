"""ITMT PID gains from /etc/thermald/itmt-pid.xml, scaled with the PL1
range of the ITMT entry.

ITMT entry on TCPU at 60 C, PL1 down to 25 W from the PPCC maximum 35 W:
a 10 W range. itmt-pid.xml sets Kp 60, a reference range of 5 W and a
scale of at most 1.5, has an invalid Kd and an unknown element:

  - Kp 60 * 1.5 (10 W / 5 W = 2, clamped to 1.5) = 90
  - Kd: default 1000 * 1.5 = 1500, the invalid value is reported
  - the unknown element is reported
  - TCPU 65 C: PL1 comes down to 25 W.
"""

import gddv
from platforms import ppcc, rapl

CONFIG = """<?xml version="1.0"?>
<ItmtPid>
	<Enable>1</Enable>
	<Kp>60</Kp>
	<Kd>fast</Kd>
	<ReferenceRange>5000</ReferenceRange>
	<ScaleMax>1.5</ScaleMax>
	<Gain>2</Gain>
</ItmtPid>
"""

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "thermald_args": ["--adaptive"],
    "conf_files": {"itmt-pid.xml": CONFIG},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 25000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    log = g.log()
    g.check("invalid Kd \"fast\"" in log, "invalid Kd not reported")
    g.check("unknown element Gain" in log, "unknown element not reported")
    g.check("gain scale 1.50" in log, "gain scale not clamped to 1.5")
    g.check("pid valid 90.000000:0.000000:1500.000000" in log,
            "Kp 90 / Kd 1500 expected (60 and the default 1000, scaled 1.5)")

    g.set_temp("TCPU", 65)
    g.wait_until(lambda: g.pl1("mmio") == 25, 60,
                 lambda: "TCPU 65 C: PL1 down to 25 W (is %.1f W)" % g.pl1("mmio"))
    g.check(g.stop_thermald() == 0, "exit status")
