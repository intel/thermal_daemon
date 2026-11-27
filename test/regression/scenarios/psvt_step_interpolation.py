"""PSVT step control: the PL1 limit is interpolated between the entries,
approached by step_size * limit_coeff per sampling period and released by
step_size * unlimit_coeff.

PPCC: PL1 10-35 W. PSVT for SEN2, all entries with a step size of 500 mW,
limit coefficient 4 (2 W per period), unlimit coefficient 2 (1 W per
period), sampling period 1 s:

    50 C   30 W
    52 C   20 W
    55 C   MIN (the PPCC minimum, 10 W)

  1. SEN2 40 C: PL1 at the PPCC maximum, 35 W.
  2. SEN2 51 C, package power 35 W: above the 30 W limit of the 50 C
     entry, so PL1 is set to 30 W at once, then steps of 2 W down to the
     limit interpolated half way between 30 and 20 W: 25 W.
  3. SEN2 53.5 C: the 52 C entry is reached with the power above its
     20 W: PL1 20 W at once, then down to half way between 20 and 10 W:
     15 W.
  4. SEN2 51 C: PL1 released to 25 W, at most 1 W per write.
  5. SEN2 40 C: PL1 back at 35 W.
  6. Demand 22 W, below the 30 W limit, SEN2 51 C: PL1 is set to the
     package power, 22 W, then released towards the interpolated 25 W at
     most 1 W per write.
  7. SIGTERM: the power-on PL1 is restored.
"""

import gddv
from platforms import POWER_ON_PL1_W, ppcc, rapl


def entry(celsius, limit):
    e = gddv.psv(gddv.TCPU, "\\_SB_.SEN2", celsius, limit=limit,
                 sample_period=10, step_size=500, limit_coeff=4,
                 unlimit_coeff=2)
    e["temperature_dk"] = celsius * 10 + 2732
    return e


PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [entry(50, 30000), entry(52, 20000),
                             entry(55, "MIN")]},
    },
}


def expect_pl1(g, watts, what, timeout=40):
    g.wait_until(lambda: abs(g.pl1("mmio") - watts) < 0.01, timeout,
                 lambda: "%s: MMIO PL1 %g W (is %.2f W)"
                 % (what, watts, g.pl1("mmio")))


def steps(g, since):
    """Changes between consecutive MMIO PL1 writes after `since` writes."""
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][since - 1:]
    return [round(b - a, 3) for a, b in zip(w, w[1:]) if b != a]


def run(g):
    g.wait_ready()
    expect_pl1(g, 35, "SEN2 40 C, no limit")

    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 51)
    expect_pl1(g, 25, "SEN2 51 C, interpolated limit")
    d = steps(g, n)
    g.check(d[:1] == [-5] and all(-2.01 <= x < 0 for x in d[1:]),
            "set to the 30 W entry limit, then steps of at most 2 W "
            "expected: %s" % d)

    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 53.5)
    expect_pl1(g, 15, "SEN2 53.5 C, interpolated limit")
    d = steps(g, n)
    g.check(d[:1] == [-5] and all(-2.01 <= x < 0 for x in d[1:]),
            "set to the 20 W entry limit, then steps of at most 2 W "
            "expected: %s" % d)
    writes = [w["value"] / 1e6 for w in g.rapl_writes("mmio")]
    g.check(min(writes) >= 15 - 0.01,
            "PL1 written below the interpolated limit: %s" % writes)

    n = len(g.rapl_writes("mmio"))
    t0 = g.now()
    g.set_temp("SEN2", 51)
    expect_pl1(g, 25, "SEN2 back at 51 C")
    d = steps(g, n)
    g.check(d and all(0 < x <= 1.01 for x in d),
            "release steps of at most 1 W expected: %s" % d)
    g.check(g.now() - t0 >= 8,
            "released 15 -> 25 W in %.1f s, expected 1 W per second"
            % (g.now() - t0))

    g.set_temp("SEN2", 40)
    expect_pl1(g, 35, "SEN2 40 C again")

    g.set_demand(22)
    g.hold(lambda: True, 4, "")     # package power settles at 22 W
    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 51)
    expect_pl1(g, 25, "demand 22 W, SEN2 51 C")
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][n:]
    g.check(w and 21.5 <= w[0] <= 22.5,
            "power below the entry limit: PL1 set to the 22 W package "
            "power first: %s" % w)
    d = steps(g, n + 1)
    g.check(all(0 < x <= 1.01 for x in d),
            "then released at most 1 W per write: %s" % d)

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
