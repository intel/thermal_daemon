"""PSVT passive control of PL1 within the PPCC range, and restore.

Firmware leaves PL1 at 15 W. The GDDV has a PPCC for TCPU (PL1 10-35 W)
and one PSVT entry: throttle TCPU when SEN2 reaches 50 C.

  1. At start thermald sets the MMIO PL1 to the PPCC maximum, 35 W.
  2. SEN2 at 60 C: PL1 goes down, never below the PPCC minimum.
  3. SEN2 back at 40 C: PL1 returns to 35 W.
  4. SIGTERM: thermald exits cleanly and puts the power-on PL1 back.
"""

import gddv

PPCC_MIN_W = 10
PPCC_MAX_W = 35
POWER_ON_PL1_W = 15

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": {"pl1_uw": POWER_ON_PL1_W * 1000000, "max_uw": 64000000,
             "demand_uw": 45000000},
    "gddv": {
        "ppcc": {"TCPU.D0": gddv.ppcc(
            pl1_min_mw=PPCC_MIN_W * 1000, pl1_max_mw=PPCC_MAX_W * 1000,
            pl1_twin_min_ms=28000, pl1_twin_max_ms=32000, pl1_step_mw=1000,
            pl2_min_mw=35000, pl2_max_mw=64000, pl2_twin_min_ms=2,
            pl2_twin_max_ms=2, pl2_step_mw=1000)},
        "psvt": {"IETM.D0": [
            gddv.psv("\\_SB_.PC00.TCPU", "\\_SB_.SEN2", 50, limit="MIN",
                     sample_period=10),
        ]},
    },
}


def run(g):
    g.wait_ready()
    g.check("IETM_D0 processed" in g.log(), "default IETM.D0 PSVT not installed")

    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 20,
                 lambda: "MMIO PL1 at the PPCC maximum %d W (is %.1f W)"
                 % (PPCC_MAX_W, g.pl1("mmio")))

    # Hot: PL1 must come down and stay within the PPCC range
    g.set_temp("SEN2", 60)
    g.wait_until(lambda: g.pl1("mmio") <= PPCC_MIN_W + 5, 90,
                 lambda: "MMIO PL1 throttled to near the PPCC minimum "
                 "(is %.1f W)" % g.pl1("mmio"))
    throttled = [w["value"] / 1e6 for w in g.rapl_writes("mmio")]
    g.check(min(throttled) >= PPCC_MIN_W,
            "PL1 written below the PPCC minimum: %s" % throttled)

    # Normal again: PL1 back at the PPCC maximum
    g.set_temp("SEN2", 40)
    g.wait_until(lambda: g.pl1("mmio") == PPCC_MAX_W, 90,
                 lambda: "MMIO PL1 restored to %d W (is %.1f W)"
                 % (PPCC_MAX_W, g.pl1("mmio")))

    status = g.stop_thermald()
    g.check(status == 0, "thermald exit status %s" % status)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
