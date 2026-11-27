"""Non-adaptive thermal-conf.xml with several passive trips in one zone
(issue #616): a PL1 "staircase" on CPU and skin temperature, and a PL2
trip with a large hysteresis.

Uses configs/pl1_staircase.xml, the example for the issue, with the
product name matching any platform and I_VTS1 as a test thermal zone.

  - several passive trips in one new zone are all kept
  - PL1 steps down as trips turn on, and when the most restrictive trip
    turns off PL1 goes to the next active trip's value, not to the 30 W
    maximum
  - PL2 drops to 21 W at 99 C and stays there until below 74 C
"""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "configs", "pl1_staircase.xml")) as f:
    CONFIG = f.read()
CONFIG = CONFIG.replace("<ProductName>Surface Book 3</ProductName>",
                        "<ProductName>*</ProductName>")
CONFIG = re.sub(r"\s*<ThermalSensors>.*?</ThermalSensors>", "", CONFIG,
                flags=re.S)

PLATFORM = {
    "zones": ["x86_pkg_temp", "I_VTS1"],
    "initial_temps": {"x86_pkg_temp": 60, "I_VTS1": 35},
    "rapl": {"pl1_uw": 30000000, "pl2_uw": 61000000, "max_uw": 64000000,
             "demand_uw": 45000000},
    "thermald_args": ["--ignore-default-control",
                      "--config-file=/etc/thermald/thermal-conf.xml"],
    "conf_files": {"thermal-conf.xml": CONFIG},
}


def expect_pl1(g, watts, what, timeout=20):
    g.wait_until(lambda: g.pl1("msr") == watts, timeout,
                 lambda: "%s: PL1 %g W (is %.1f W)" % (what, watts, g.pl1("msr")))


def run(g):
    g.wait_ready()
    g.check("cpu_and_skin" in g.log(), "XML zone not loaded")
    expect_pl1(g, 30, "all cool")

    # CPU steps down
    g.set_temp("x86_pkg_temp", 87)
    expect_pl1(g, 24, "CPU 87 C")
    g.set_temp("x86_pkg_temp", 93)
    expect_pl1(g, 18, "CPU 93 C")
    g.set_temp("x86_pkg_temp", 98)
    expect_pl1(g, 12, "CPU 98 C")

    # ... and back up one step at a time, never to 30 W while hot
    g.set_temp("x86_pkg_temp", 93)          # 97 C trip off (< 94 C)
    expect_pl1(g, 18, "CPU back to 93 C")
    g.hold(lambda: g.pl1("msr") == 18, 5,
           lambda: "CPU 93 C: PL1 left 18 W (%.1f W)" % g.pl1("msr"))
    g.set_temp("x86_pkg_temp", 86)          # 92 C trip off (< 89 C)
    expect_pl1(g, 24, "CPU back to 86 C")
    g.set_temp("x86_pkg_temp", 70)          # 85 C trip off (< 82 C)
    expect_pl1(g, 30, "CPU cool again")

    # Skin and CPU together: the most restrictive wins, release in steps
    g.set_temp("x86_pkg_temp", 87)          # 24 W
    g.set_temp("I_VTS1", 46)                # 14 W
    expect_pl1(g, 14, "skin 46 C + CPU 87 C", timeout=30)
    g.set_temp("I_VTS1", 43.5)              # 45 C trip off, 43 C on: 20 W
    expect_pl1(g, 20, "skin 43.5 C + CPU 87 C", timeout=30)
    g.set_temp("I_VTS1", 40)                # skin off: CPU's 24 W
    expect_pl1(g, 24, "skin cool + CPU 87 C", timeout=30)

    # PL2: 21 W at 99 C, held while above 74 C
    g.set_temp("x86_pkg_temp", 100)
    g.wait_until(lambda: g.pl2("msr") == 21, 20,
                 lambda: "CPU 100 C: PL2 21 W (is %.1f W)" % g.pl2("msr"))
    g.set_temp("x86_pkg_temp", 80)
    g.hold(lambda: g.pl2("msr") == 21, 6,
           lambda: "CPU 80 C: PL2 released early (%.1f W)" % g.pl2("msr"))
    g.set_temp("x86_pkg_temp", 70)
    g.wait_until(lambda: g.pl2("msr") == 61, 20,
                 lambda: "CPU 70 C: PL2 back to 61 W (is %.1f W)" % g.pl2("msr"))
