"""Random D-Bus calls to thermald running the adaptive engine.

Calls every method of org.freedesktop.thermald (except Terminate) with
random valid and invalid arguments, see lib/dbus_fuzz.py. thermald must
answer every call, keep running, report no ASan, UBSan or leak error
(the scenario runs the sanitizer build, test/unit/build/thermald, unless
THERMALD_BIN is set) and exit cleanly.

The calls are repeatable: seed 1, 3000 calls by default, or set
THD_DBUS_FUZZ_SEED and THD_DBUS_FUZZ_CALLS. The calls made are in
dbus_calls.log in the scenario's output.

SetUserMaxTemperature used to send no reply on success, so the caller
waited for the D-Bus timeout.
"""

import dbus_fuzz
from dbus_fuzz_platform import FEATURES_DBUS, fuzz_settings, names, paths
import gddv
from platforms import ppcc, rapl


PLATFORM = {
    "dbus": True,
    "sanitized": True,
    "dbus_fuzz": fuzz_settings(1),
    "thermald_args": ["--adaptive", "--dbus-enable"],
    "conf_files": {"thermald-features.xml": FEATURES_DBUS},
    "zones": ["SEN2", "SEN3"],
    "initial_temps": {"SEN2": 35, "SEN3": 40},
    "cdevs": ["intel_powerclamp", "Fan"],
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)]},
        "vsct": {"VR1.D0": [("\\_SB_.SEN3", 500, 1000)]},
        "vspt": {"VR1.D0": [(0, 1)]},
    },
}


def run(g):
    g.wait_ready()
    f = g.platform["dbus_fuzz"]
    counts = dbus_fuzz.run(g, f["calls"], f["seed"], names(PLATFORM), paths())
    g.check(len(counts) > 20, "only %d methods called" % len(counts))
    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    dbus_fuzz.sanitizer_check(g, ["(at exit)"])
