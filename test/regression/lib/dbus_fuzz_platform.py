"""Shared parts of the dbus_fuzz scenarios' platforms."""

import glob
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

with open(os.path.join(REPO, "data", "thermald-features.xml")) as f:
    # The shipped default has D-Bus control disabled
    FEATURES_DBUS = f.read().replace("<DbusControl> 0 </DbusControl>",
                                     "<DbusControl> 1 </DbusControl>")


def fuzz_settings(seed):
    """Seed and number of calls; THD_DBUS_FUZZ_SEED and
    THD_DBUS_FUZZ_CALLS override them (read on the host, the guest gets
    them in platform.json)."""
    return {"seed": int(os.environ.get("THD_DBUS_FUZZ_SEED", seed)),
            "calls": int(os.environ.get("THD_DBUS_FUZZ_CALLS", 3000))}


def names(platform):
    """Zone, sensor and cooling device names thermald will know."""
    out = list(platform.get("zones", [])) + list(platform.get("cdevs", []))
    out += ["rapl_controller", "rapl_controller_mmio", "intel_powerclamp",
            "Processor", "B0D4", "TCPU", "VR1", "VR1.D0", "SEN2", "SEN3",
            "x86_pkg_temp", "cpufreq", "dbus_zone", "dbus_sensor", "dbus_cdev"]
    return sorted(set(out))


def paths():
    """sysfs paths of the VM's thermal zones and cooling devices (the
    test module's), evaluated in the guest."""
    out = []
    for d in sorted(glob.glob("/sys/class/thermal/thermal_zone*")
                    + glob.glob("/sys/class/thermal/cooling_device*")):
        out.append(d)
        for attr in ("temp", "type", "cur_state", "max_state",
                     "trip_point_0_temp"):
            if os.path.exists(os.path.join(d, attr)):
                out.append(os.path.join(d, attr))
    return out
