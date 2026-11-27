"""ITMT with the PID enabled in itmt-pid.xml: PL1 is controlled by a PID
within the ITMT range once the trip is exceeded, and released gradually.

ITMT v1, one entry on TCPU: above 60 C (written as 59.5 C, the trip) PL1
may go down to 25 W, no upper limit ("MAX"). PPCC: PL1 10-35 W. The PID
aims half a degree below the trip, at 59 C; the ITMT hysteresis is 2 C.

  1. TCPU 45 C: PL1 at the PPCC maximum, 35 W, the trip has a PID.
  2. TCPU 65 C, more than 1 C above the trip: PL1 comes down to 25 W in
     several steps within 40 s (at least the default controller's doubling
     steps), never below it.
  3. TCPU 59.4 C, fallen 5.6 C but still above the set point: PL1 isn't
     raised.
  4. TCPU 58 C, within the hysteresis and below the set point: the PID
     releases part of the limit, PL1 between 25 and 35 W.
  5. TCPU 45 C, below the hysteresis: PL1 back at 35 W.
  6. SIGTERM: the power-on PL1 is restored.
"""

import gddv
from platforms import ITMT_PID_ON, POWER_ON_PL1_W, ppcc, rapl

PLATFORM = {
    "zones": ["TCPU"],
    "initial_temps": {"TCPU": 45},
    "rapl": rapl(),
    "thermald_args": ["--adaptive"],
    "conf_files": {"itmt-pid.xml": ITMT_PID_ON},
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "apct": [(1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "Itmt", gddv.IETM, "ITMT", "IETM.D0")],
        "itmt": {"IETM.D0": [(gddv.TCPU, 60, 25000, "MAX")]},
        "idsp": [gddv.ITMT_UUID],
    },
}


def pl1(g):
    return g.pl1("mmio")


def run(g):
    g.wait_ready()
    g.wait_log("set_int3400 ITMT target IETM.D0")
    g.wait_until(lambda: pl1(g) == 35, 60,
                 lambda: "TCPU 45 C: PL1 35 W (is %.1f W)" % pl1(g))
    g.check("pid valid" in g.log(), "ITMT trip installed without a PID")

    n = len(g.rapl_writes("mmio"))
    g.set_temp("TCPU", 65)
    g.wait_until(lambda: pl1(g) == 25, 40,
                 lambda: "TCPU 65 C: PL1 down to 25 W within 40 s (is %.1f W)"
                 % pl1(g))
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][n:]
    g.check(min(w) >= 25, "PL1 written below the ITMT limit: %s" % w)
    g.check(len(set(v for v in w if 25 < v < 35)) >= 2,
            "PL1 not controlled down in steps: %s" % w)

    g.set_temp("TCPU", 59.4)
    g.hold(lambda: pl1(g) == 25, 15,
           lambda: "TCPU 59.4 C, above the set point: PL1 raised to %.1f W"
           % pl1(g))

    g.set_temp("TCPU", 58)
    g.hold(lambda: True, 15, "")
    g.check(25 < pl1(g) < 35,
            "TCPU 58 C, below the set point within the hysteresis: PL1 "
            "%.1f W, expected partly released (25-35 W)" % pl1(g))

    g.set_temp("TCPU", 45)
    g.wait_until(lambda: pl1(g) == 35, 90,
                 lambda: "TCPU 45 C: PL1 back at 35 W (is %.1f W)" % pl1(g))

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.check(pl1(g) == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the power-on %d W"
            % (pl1(g), POWER_ON_PL1_W))
