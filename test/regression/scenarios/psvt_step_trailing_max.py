"""PSVT step control: a last entry with limit MAX is no limit at its
temperature and the full limit (PPCC minimum) 1 C above it, interpolated.
The limit setting value of a MAX entry is PL1 max, so reaching the full
limit doesn't cut the limit at once: it steps by step size * limit
coefficient per period.

PPCC: PL1 10-35 W. One PSVT entry for SEN2: 50 C, MAX, step 500 mW,
limit coefficient 10 (5 W per period), unlimit coefficient 10.

  1. SEN2 40 C: PL1 35 W.
  2. SEN2 50.5 C: half way between no limit and the minimum: 22.5 W.
  3. SEN2 52 C: PL1 down to the PPCC minimum, 10 W, in steps of at most
     5 W.
  4. SEN2 40 C: PL1 back at 35 W.
"""

import gddv
from platforms import ppcc, rapl

e = gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit="MAX", sample_period=10,
             step_size=500, limit_coeff=10, unlimit_coeff=10)
e["temperature_dk"] = 50 * 10 + 2732

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 40},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [e]},
    },
}


def expect_pl1(g, watts, what, timeout=30):
    g.wait_until(lambda: abs(g.pl1("mmio") - watts) < 0.01, timeout,
                 lambda: "%s: MMIO PL1 %g W (is %.2f W)"
                 % (what, watts, g.pl1("mmio")))


def run(g):
    g.wait_ready()
    expect_pl1(g, 35, "SEN2 40 C, no limit")
    g.set_temp("SEN2", 50.5)
    expect_pl1(g, 22.5, "SEN2 50.5 C")
    n = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 52)
    expect_pl1(g, 10, "SEN2 52 C")
    w = [x["value"] / 1e6 for x in g.rapl_writes("mmio")][n - 1:]
    d = [round(b - a, 3) for a, b in zip(w, w[1:]) if b != a]
    g.check(d and all(-5.01 <= x < 0 for x in d),
            "full limit reached in steps of at most 5 W expected: %s" % d)
    g.set_temp("SEN2", 40)
    expect_pl1(g, 35, "SEN2 40 C again")
    g.check(g.stop_thermald() == 0, "exit status")
