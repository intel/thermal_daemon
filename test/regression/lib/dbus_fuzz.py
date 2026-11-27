"""Random calls to every thermald D-Bus method, for the dbus_fuzz
scenarios.

The methods and their argument types are read from
src/thd_dbus_interface.xml, so a new method is covered without changing
this file. Arguments mix valid values (names of the scenario's zones,
sensors and cooling devices, sysfs paths of the test module) with
invalid ones: empty, "..", path traversal, printf conversions, very long
or non-ASCII strings, 0, -1, INT_MIN/INT_MAX, UINT32_MAX, NaN and
infinity.

Paths are kept to the test module, files that don't exist and paths
outside /sys (which thermald rejects): an added sensor or cooling device
must not make thermald write to a real sysfs attribute of the VM.

Terminate is not called; Reinit only occasionally (it restarts the
engine and returns when the new one runs, after about 3 s).
"""

import os
import random
import time
import xml.etree.ElementTree as ET

INTERFACE = "org.freedesktop.thermald"
OBJECT = "/org/freedesktop/thermald"
SKIP = {"Terminate"}
RARE = {"Reinit": 0.01}

INT_MIN, INT_MAX, UINT_MAX = -2 ** 31, 2 ** 31 - 1, 2 ** 32 - 1


def methods(repo):
    """[(name, [signature of each in argument])]"""
    root = ET.parse(os.path.join(repo, "src", "thd_dbus_interface.xml")).getroot()
    out = []
    for m in root.iter("method"):
        args = [a.get("type") for a in m.iter("arg")
                if a.get("direction", "in") == "in"]
        out.append((m.get("name"), args))
    return out


class Generator:
    def __init__(self, rng, names, paths):
        self.rng = rng
        self.names = list(names)
        self.paths = list(paths)

    def string(self):
        r = self.rng
        pick = r.random()
        if pick < 0.35 and self.names:
            return r.choice(self.names)
        if pick < 0.55 and self.paths:
            return r.choice(self.paths)
        return r.choice([
            "", " ", ".", "..", "../..", "../../../etc/passwd",
            "/sys/../etc/passwd", "/sys/kernel/thermald_test/../../../etc/shadow",
            "/sys/nonexistent", "/etc/thermald/thermal-conf.xml", "/tmp",
            "%s%s%s%n", "%x%x%x%x", "\t\n", "zone name", "SEN2 ",
            "x" * 128, "x" * 129, "y" * 5000, "é中\U0001f600",
            "ENERGY_CONSERVE", "PERFORMANCE", "FALLBACK", "rapl_controller",
            "intel_powerclamp", "cpufreq", "Processor", "B0D4", "TCPU",
            str(r.randint(INT_MIN, INT_MAX)),
            "".join(chr(r.randint(1, 0x7f)) for _ in range(r.randint(1, 40))),
        ])

    def int32(self):
        r = self.rng
        return r.choice([0, 1, -1, 2, 10, 100, 1000, INT_MAX, INT_MIN,
                         INT_MAX - 1, INT_MIN + 1, r.randint(-200000, 200000),
                         r.randint(INT_MIN, INT_MAX)])

    def uint32(self):
        r = self.rng
        return r.choice([0, 1, 2, 3, 100, 1000, 45000, 85000, 105000,
                         UINT_MAX, UINT_MAX - 1, 2 ** 31, 2 ** 31 - 1,
                         r.randint(0, 200000), r.randint(0, UINT_MAX)])

    def double(self):
        r = self.rng
        return r.choice([0.0, 1.0, -1.0, 0.5, 1000.0, 1000.0001, -1000.0001,
                         1e308, -1e308, 5e-324, float("nan"), float("inf"),
                         float("-inf"), r.uniform(-2000, 2000)])

    def arg(self, sig):
        import dbus
        return {"s": lambda: dbus.String(self.string()),
                "i": lambda: dbus.Int32(self.int32()),
                "u": lambda: dbus.UInt32(self.uint32()),
                "d": lambda: dbus.Double(self.double())}[sig]()


def run(g, calls, seed, names, paths):
    """Make `calls` random method calls; fail if thermald stops replying,
    exits, or a sanitizer reports an error. Returns {method: count}."""
    import dbus

    rng = random.Random(seed)
    gen = Generator(rng, names, paths)
    bus = g._bus()
    obj = bus.get_object(INTERFACE, OBJECT)
    table = [(n, a) for n, a in methods(g.repo) if n not in SKIP]
    counts = {}
    history = []
    journal = open(os.path.join(g.outdir, "dbus_calls.log"), "w")
    journal.write("seed %d\n" % seed)

    for i in range(calls):
        name, sigs = rng.choice(table)
        if name in RARE and rng.random() > RARE[name]:
            continue
        args = [gen.arg(s) for s in sigs]
        line = "%d %s(%s)" % (i, name, ", ".join(repr(a) for a in args))
        journal.write(line + "\n")
        journal.flush()
        history = (history + [line])[-10:]
        counts[name] = counts.get(name, 0) + 1
        try:
            getattr(obj, name)(*args, dbus_interface=INTERFACE, timeout=20)
        except dbus.exceptions.DBusException as e:
            err = e.get_dbus_name() or ""
            # A rejected call is expected; no reply means thermald hung
            # or died
            if (err in ("org.freedesktop.DBus.Error.NoReply",
                        "org.freedesktop.DBus.Error.ServiceUnknown",
                        "org.freedesktop.DBus.Error.Disconnected",
                        "org.freedesktop.DBus.Error.Timeout")
                    or not g.thermald_running()):
                g.check(False, "thermald stopped answering (%s) after:\n  %s"
                        % (err, "\n  ".join(history)))
        g.check(g.thermald_running(),
                "thermald exited (status %s) after:\n  %s"
                % (g.proc.returncode, "\n  ".join(history)))
        if i % 100 == 0:
            sanitizer_check(g, history)
    journal.close()
    time.sleep(1)
    sanitizer_check(g, history)
    return counts


def sanitizer_check(g, history):
    log = g.log()
    for marker in ("ERROR: AddressSanitizer", "runtime error:",
                   "ERROR: LeakSanitizer"):
        if marker in log:
            g.check(False, "%s in thermald's log after:\n  %s"
                    % (marker, "\n  ".join(history)))
