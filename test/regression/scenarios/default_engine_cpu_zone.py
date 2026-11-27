"""Default engine without a data vault or thermal-conf.xml: the CPU zone
thermald creates by itself, as on most machines without DTT tables.

The test module provides the platform: an x86_pkg_temp zone, a coretemp
hwmon device (temp1_max 80 C, temp1_crit 100 C), RAPL, an intel_pstate
sysfs, a cpufreq driver (800 MHz - 3.2 GHz) and an intel_powerclamp
cooling device; the VM has ACPI Processor cooling devices.

  1. thermald builds the CPU zone from coretemp: passive trip at
     max + (crit - max) / 2 = 90 C, with the cooling devices in the order
     of thermal-cpu-cdev-order.xml: rapl_controller, intel_pstate,
     intel_powerclamp, cpufreq, Processor.
  2. x86_pkg_temp 95 C: the devices throttle one after the other, each to
     its limit before the next one starts: PL1 down, max_perf_pct down
     (turbo off on the way), powerclamp up to 100, the cpufreq maximum
     down to 800 MHz, then Processor.
  3. 85 C, below the passive trip but above the polling trip (81 C): the
     devices are released in the reverse order, each fully before the
     next one: Processor, cpufreq, powerclamp, intel_pstate (turbo on
     again), PL1 back at the RAPL maximum.
  4. SIGTERM: PL1 back at its power-on value, the other devices
     unthrottled.
"""

import time

from default_engine import (MAX_KHZ, MIN_KHZ, check_sequence,
                            processor_state, writes)
from platforms import POWER_ON_PL1_W, rapl

ORDER = ["rapl_controller", "intel_pstate", "intel_powerclamp", "cpufreq",
         "Processor"]
RAPL_MAX_W = 64

PLATFORM = {
    "zones": ["x86_pkg_temp"],
    "initial_temps": {"x86_pkg_temp": 50},
    "cdevs": ["intel_powerclamp"],
    "rapl": rapl(max_uw=RAPL_MAX_W * 1000000),
    "coretemp": {"max_c": 80, "crit_c": 100},
    "intel_pstate": True,
    "cpufreq": True,
    "thermald_args": ["--ignore-cpuid-check"],
}


def run(g):
    g.wait_ready()
    log = g.log()
    g.check("Core temp DTS :critical 100000, max 80000, psv 90000" in log,
            "trips not computed from coretemp max 80 C / crit 100 C")
    g.check("type:passive temp:90000" in log, "no passive trip at 90 C")
    for i, name in enumerate(ORDER):
        g.check("cdev[%d] %s," % (i, name) in log,
                "cooling device %d of the CPU zone isn't %s" % (i, name))
    g.check(processor_state() == 0, "Processor throttled at 50 C")

    # 2. Throttle in order
    heat_ms = int(time.monotonic() * 1000)
    g.set_temp("x86_pkg_temp", 95)
    g.wait_until(lambda: g.cpufreq_max_khz() == MIN_KHZ
                 and processor_state() > 0, 150,
                 lambda: "all devices throttled (cpufreq max %d kHz, "
                 "Processor %d)" % (g.cpufreq_max_khz(), processor_state()))
    w = writes(g, heat_ms)
    check_sequence(g, w, ORDER[:4], "throttling")
    pl1 = [v for _, v in w["rapl_controller"]]
    g.check(pl1 == sorted(pl1, reverse=True) and pl1[-1] < RAPL_MAX_W * 1e6,
            "PL1 not stepped down: %s" % pl1)
    g.check(g.pstate() <= 10 and g.pstate("no_turbo") == 1,
            "intel_pstate not at its limit: max_perf_pct %d no_turbo %d"
            % (g.pstate(), g.pstate("no_turbo")))
    g.check(g.cdev_state("intel_powerclamp") == 100,
            "powerclamp at %d, not 100" % g.cdev_state("intel_powerclamp"))

    # 3. Release in the reverse order
    cool_ms = int(time.monotonic() * 1000)
    g.set_temp("x86_pkg_temp", 85)
    g.wait_until(lambda: g.pl1("msr") == RAPL_MAX_W, 150,
                 lambda: "PL1 back at %d W (is %.1f W)"
                 % (RAPL_MAX_W, g.pl1("msr")))
    w = writes(g, cool_ms)
    check_sequence(g, w, ORDER[3::-1], "releasing")
    g.check(processor_state() == 0, "Processor still throttled")
    g.check(g.cpufreq_max_khz() == MAX_KHZ,
            "cpufreq max %d kHz" % g.cpufreq_max_khz())
    g.check(g.cdev_state("intel_powerclamp") == 0, "powerclamp not released")
    g.check(g.pstate() == 100 and g.pstate("no_turbo") == 0,
            "intel_pstate not released: max_perf_pct %d no_turbo %d"
            % (g.pstate(), g.pstate("no_turbo")))
    first_pstate_release = w["intel_pstate"][0][0]
    no_turbo = [x for x in g.cdev_writes("intel_pstate", "no_turbo")
                if x["ms"] >= cool_ms]
    g.check(no_turbo and no_turbo[-1]["value"] == 0
            and no_turbo[-1]["ms"] >= first_pstate_release,
            "turbo not enabled again while releasing: %s" % no_turbo)

    # 4. Exit
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.check(g.pl1("msr") == POWER_ON_PL1_W,
            "PL1 %.1f W after exit, power-on value %d W"
            % (g.pl1("msr"), POWER_ON_PL1_W))
    g.check(g.pstate() == 100 and g.cpufreq_max_khz() == MAX_KHZ
            and g.cdev_state("intel_powerclamp") == 0,
            "a cooling device still throttled after exit")
