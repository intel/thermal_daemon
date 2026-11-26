"""Default engine with a custom thermal-cpu-cdev-order.xml: the CPU zone
uses only the listed cooling devices, in the listed order.

The order file lists cpufreq, then intel_powerclamp. RAPL and
intel_pstate exist but aren't listed.

  1. The CPU zone's passive trip has cpufreq, then intel_powerclamp.
  2. x86_pkg_temp 95 C: the cpufreq maximum goes down to 800 MHz first,
     then powerclamp up to 100. PL1 and max_perf_pct are never written.
  3. 85 C: released in the reverse order.
"""

import time

from default_engine import MAX_KHZ, MIN_KHZ, check_sequence, writes
from platforms import rapl

ORDER = ["cpufreq", "intel_powerclamp"]

CDEV_ORDER = """<?xml version="1.0"?>
<CoolingDeviceOrder>
	<CoolingDevice>cpufreq</CoolingDevice>
	<CoolingDevice>intel_powerclamp</CoolingDevice>
</CoolingDeviceOrder>
"""

PLATFORM = {
    "zones": ["x86_pkg_temp"],
    "initial_temps": {"x86_pkg_temp": 50},
    "cdevs": ["intel_powerclamp"],
    "rapl": rapl(),
    "coretemp": {"max_c": 80, "crit_c": 100},
    "intel_pstate": True,
    "cpufreq": True,
    "thermald_args": ["--ignore-cpuid-check"],
    "conf_files": {"thermal-cpu-cdev-order.xml": CDEV_ORDER},
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("CDEVS order specified in thermal-cpu-cdev-order.xml" in log,
            "the order file wasn't used")
    g.check("type:passive temp:90000" in log and "cdev size:2" in log,
            "the CPU zone's passive trip doesn't have 2 devices")
    for i, name in enumerate(ORDER):
        g.check("cdev[%d] %s," % (i, name) in log,
                "cooling device %d of the CPU zone isn't %s" % (i, name))

    heat_ms = int(time.monotonic() * 1000)
    pl1 = g.pl1("msr")
    g.set_temp("x86_pkg_temp", 95)
    g.wait_until(lambda: g.cdev_state("intel_powerclamp") == 100, 120,
                 lambda: "powerclamp at 100 (is %d)"
                 % g.cdev_state("intel_powerclamp"))
    w = writes(g, heat_ms)
    check_sequence(g, w, ORDER, "throttling")
    g.check(g.cpufreq_max_khz() == MIN_KHZ, "cpufreq not at its minimum")
    g.check(not w["rapl_controller"] and not w["intel_pstate"],
            "unlisted devices written: PL1 %s, max_perf_pct %s"
            % (w["rapl_controller"], w["intel_pstate"]))
    g.check(g.pl1("msr") == pl1 and g.pstate() == 100,
            "PL1 or max_perf_pct changed")

    cool_ms = int(time.monotonic() * 1000)
    g.set_temp("x86_pkg_temp", 85)
    g.wait_until(lambda: g.cpufreq_max_khz() == MAX_KHZ, 120,
                 "cpufreq maximum back at %d kHz" % MAX_KHZ)
    check_sequence(g, writes(g, cool_ms), ORDER[::-1], "releasing")
    g.check(g.cdev_state("intel_powerclamp") == 0, "powerclamp not released")

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
