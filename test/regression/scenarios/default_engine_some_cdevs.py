"""Default engine on a machine with only some of the CPU cooling devices:
no RAPL and no intel_pstate (e.g. intel_pstate=disable with a cpufreq
driver instead).

  1. The CPU zone has the devices that exist, in the default order:
     intel_powerclamp, cpufreq, Processor.
  2. x86_pkg_temp 95 C: powerclamp up to 100, then the cpufreq maximum
     down to 800 MHz, then Processor.
  3. 85 C: released in the reverse order.
  4. SIGTERM: exit status 0, nothing left throttled.
"""

import time

from default_engine import (MAX_KHZ, MIN_KHZ, check_sequence,
                            processor_state, writes)

ORDER = ["intel_powerclamp", "cpufreq", "Processor"]

PLATFORM = {
    "zones": ["x86_pkg_temp"],
    "initial_temps": {"x86_pkg_temp": 50},
    "cdevs": ["intel_powerclamp"],
    "coretemp": {"max_c": 80, "crit_c": 100},
    "cpufreq": True,
    "thermald_args": ["--ignore-cpuid-check"],
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("cdev size:3" in log, "the CPU zone's trip doesn't have 3 devices")
    for i, name in enumerate(ORDER):
        g.check("cdev[%d] %s," % (i, name) in log,
                "cooling device %d of the CPU zone isn't %s" % (i, name))

    heat_ms = int(time.monotonic() * 1000)
    g.set_temp("x86_pkg_temp", 95)
    g.wait_until(lambda: g.cpufreq_max_khz() == MIN_KHZ
                 and processor_state() > 0, 120,
                 lambda: "all devices throttled (cpufreq max %d kHz, "
                 "Processor %d)" % (g.cpufreq_max_khz(), processor_state()))
    check_sequence(g, writes(g, heat_ms), ORDER[:2], "throttling")
    g.check(g.cdev_state("intel_powerclamp") == 100, "powerclamp not at 100")

    cool_ms = int(time.monotonic() * 1000)
    g.set_temp("x86_pkg_temp", 85)
    g.wait_until(lambda: g.cdev_state("intel_powerclamp") == 0, 120,
                 "powerclamp released")
    check_sequence(g, writes(g, cool_ms), ORDER[1::-1], "releasing")
    g.check(processor_state() == 0 and g.cpufreq_max_khz() == MAX_KHZ,
            "Processor or cpufreq still throttled")

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
