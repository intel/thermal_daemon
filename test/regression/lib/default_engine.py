"""Helpers for the default engine scenarios: the CPU zone's cooling
devices, from the test module's fakes and the VM's ACPI Processor."""

import os

# The test module's default cpufreq table
MIN_KHZ, MAX_KHZ = 800000, 3200000

# Devices whose writes writes() collects, in the default order
LOGGED = ("rapl_controller", "intel_pstate", "intel_powerclamp", "cpufreq")


def processor_state():
    """Highest cur_state of the VM's ACPI Processor cooling devices."""
    states = []
    for d in os.listdir("/sys/class/thermal"):
        path = os.path.join("/sys/class/thermal", d)
        if d.startswith("cooling_device"):
            with open(path + "/type") as f:
                if f.read().strip() != "Processor":
                    continue
            with open(path + "/cur_state") as f:
                states.append(int(f.read()))
    return max(states)


def writes(g, since_ms):
    """Throttle-relevant writes after since_ms: {device: [(ms, value)]}."""
    out = {d: [] for d in LOGGED}
    for w in g.rapl_writes(domain="msr", attr="constraint_0_power"):
        if w["ms"] >= since_ms:
            out["rapl_controller"].append((w["ms"], w["value"]))
    for dev, name, attr in (("intel_pstate", "intel_pstate", "max_perf_pct"),
                            ("intel_powerclamp", "intel_powerclamp",
                             "cur_state"),
                            ("cpufreq", "cpufreq0", "max_khz")):
        for w in g.cdev_writes(device=name, attr=attr):
            if w["ms"] >= since_ms:
                out[dev].append((w["ms"], w["value"]))
    return out


def check_sequence(g, w, order, what):
    """Each device's writes all come before the next device's first."""
    for a, b in zip(order, order[1:]):
        g.check(w[a] and w[b], "%s: no writes to %s or %s (%s)"
                % (what, a, b, {k: len(v) for k, v in w.items()}))
        g.check(w[a][-1][0] <= w[b][0][0],
                "%s: %s still changing (last at %d ms) after %s started "
                "(%d ms)" % (what, a, w[a][-1][0], b, w[b][0][0]))
