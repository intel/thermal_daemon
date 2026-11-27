"""Random D-Bus calls to thermald running the default engine with a
thermal-conf.xml (configs/pl1_staircase.xml, on any platform).

As dbus_fuzz_adaptive, for the engine most D-Bus users run: zones,
trips and cooling devices from the XML configuration are added,
changed and deleted through D-Bus. Seed 2 by default.
"""

import os
import re

import dbus_fuzz
from dbus_fuzz_platform import FEATURES_DBUS, fuzz_settings, names, paths


HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "configs", "pl1_staircase.xml")) as f:
    CONFIG = f.read()
CONFIG = re.sub(r"<ProductName>[^<]*</ProductName>",
                "<ProductName>*</ProductName>", CONFIG)
CONFIG = re.sub(r"\s*<ThermalSensors>.*?</ThermalSensors>", "", CONFIG,
                flags=re.S)

PLATFORM = {
    "dbus": True,
    "sanitized": True,
    "dbus_fuzz": fuzz_settings(2),
    "zones": ["x86_pkg_temp", "I_VTS1"],
    "initial_temps": {"x86_pkg_temp": 60, "I_VTS1": 35},
    "cdevs": ["intel_powerclamp", "Fan"],
    "rapl": {"pl1_uw": 30000000, "pl2_uw": 61000000, "max_uw": 64000000,
             "demand_uw": 45000000},
    "thermald_args": ["--dbus-enable", "--ignore-cpuid-check",
                      "--config-file=/etc/thermald/thermal-conf.xml"],
    "conf_files": {"thermal-conf.xml": CONFIG,
                   "thermald-features.xml": FEATURES_DBUS},
}


def run(g):
    g.wait_ready()
    f = g.platform["dbus_fuzz"]
    counts = dbus_fuzz.run(g, f["calls"], f["seed"], names(PLATFORM), paths())
    g.check(len(counts) > 20, "only %d methods called" % len(counts))
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    dbus_fuzz.sanitizer_check(g, ["(at exit)"])
