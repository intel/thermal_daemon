"""Runs inside the test VM: set up the fake platform, start thermald, run
one scenario and write the results to the output directory.

    python3 guest.py <repo> <scenario.py> <outdir>

Exit status 0 means the scenario passed.
"""

import importlib.util
import json
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
import traceback

TEST_SYSFS = "/sys/kernel/thermald_test"
POWERCAP = "/sys/devices/virtual/powercap"
DOMAINS = {
    "msr": POWERCAP + "/intel-rapl/intel-rapl:0",
    "mmio": POWERCAP + "/intel-rapl-mmio/intel-rapl-mmio:0",
}
DBUS_SOCKET = "/run/thermald-test-dbus.sock"
DBUS_CONF = """<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>system</type>
  <listen>unix:path=%s</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow own="*"/>
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
  </policy>
</busconfig>
""" % DBUS_SOCKET
POWER_PROFILES = ("power-saver", "balanced", "performance")

# thermald is built with prefix=/, so these are its compiled-in paths.
# /etc is a throw-away overlay in the VM (vng --overlay-rwdir=/etc).
CONF_DIR = "/etc/thermald"
RUN_DIR = "/var/run/thermald"


def read(path):
    with open(path) as f:
        return f.read().strip()


def write(path, value):
    with open(path, "w") as f:
        f.write(str(value))


def sh(*cmd, check=True):
    return subprocess.run(cmd, check=check, capture_output=True, text=True)


class ScenarioFailed(AssertionError):
    pass


class Guest:
    """The API a scenario's run(g) gets."""

    def __init__(self, repo, platform, outdir):
        self.repo = repo
        self.platform = platform
        self.outdir = outdir
        self.proc = None
        # Live log on guest tmpfs: reading a file in the shared output
        # directory while it grows can fail (ENODATA on virtme-ng 1.35).
        # finish() copies it to outdir.
        os.makedirs("/run/thermald-test", exist_ok=True)
        self.log_path = "/run/thermald-test/thermald.log"
        self.trace = []
        self.t0 = time.monotonic()
        self._sampling = False
        self._log_cursor = 0
        self._rapl_log = []
        self._rapl_log_lock = threading.Lock()
        self.switches = {}
        self.helpers = []
        self.bus_address = None

    # Platform setup

    def setup(self):
        p = self.platform

        # Only the repo's default config files, never the host's
        shutil.rmtree(CONF_DIR, ignore_errors=True)
        os.makedirs(CONF_DIR)
        for name in ("thermald-features.xml", "thermal-cpu-cdev-order.xml"):
            shutil.copy(os.path.join(self.repo, "data", name), CONF_DIR)
        for name, text in p.get("conf_files", {}).items():
            write(os.path.join(CONF_DIR, name), text)
        shutil.rmtree(RUN_DIR, ignore_errors=True)
        os.makedirs(RUN_DIR)

        self._insmod()
        if p.get("gddv") is not None:
            # Print AML Debug object writes (the SSDT's IMOK logs its
            # argument) to dmesg
            write("/sys/module/acpi/parameters/debug_layer", "0xffffffff")
            write("/sys/module/acpi/parameters/debug_level", "0x2")
            sh("modprobe", "int3400_thermal")

        for zone, celsius in p.get("initial_temps", {}).items():
            self.set_temp(zone, celsius)

        # Lid / tablet-mode switches: real evdev devices via uinput,
        # created before thermald scans /dev/input
        for kind in p.get("switches", []):
            self.add_switch(kind)

        if "upower" in p or "power_profile" in p or p.get("dbus"):
            self._start_dbus()
        if "upower" in p:
            self._start_mock("upower", {
                "OnBattery": bool(p["upower"].get("on_battery", False)),
                "DaemonVersion": "1.90.2"},
                "org.freedesktop.UPower")
        if "power_profile" in p:
            self._start_mock("power_profiles_daemon", {},
                             "net.hadess.PowerProfiles")
            self.set_power_profile(p["power_profile"])

    def start_thermald(self, extra_args=None):
        binary = os.environ.get("THERMALD_BIN")
        if not binary and self.platform.get("sanitized"):
            # The ASan + UBSan build (test/unit: make thermald)
            binary = os.path.join(self.repo, "test", "unit", "build", "thermald")
        binary = binary or os.path.join(self.repo, "thermald")
        args = [binary, "--no-daemon",
                "--loglevel=debug", "--poll-interval=1"]
        args += self.platform.get("thermald_args", ["--adaptive"])
        args += extra_args or []
        env = dict(os.environ)
        if self.bus_address:
            env["DBUS_SYSTEM_BUS_ADDRESS"] = self.bus_address
        else:
            # No system bus: thermald runs without upower and
            # power-profiles-daemon, as on a minimal system
            env["DBUS_SYSTEM_BUS_ADDRESS"] = "unix:path=/nonexistent"
        self.log_file = open(self.log_path, "a")
        self.proc = subprocess.Popen(args, stdout=self.log_file,
                                     stderr=subprocess.STDOUT, env=env)
        self._start_sampling()

    def stop_thermald(self, timeout=15):
        """SIGTERM thermald like systemd does, return its exit status."""
        if not self.proc:
            return None
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
                raise ScenarioFailed("thermald didn't exit %ds after SIGTERM"
                                     % timeout)
        time.sleep(0.5)		# let the trace see the final state
        return self.proc.returncode

    def thermald_running(self):
        return self.proc is not None and self.proc.poll() is None

    # Inputs

    def set_temp(self, zone, celsius):
        write(TEST_SYSFS + "/zone_temp", "%s %d" % (zone, celsius * 1000))

    def set_demand(self, watts):
        write(TEST_SYSFS + "/rapl_demand_uw", int(watts * 1000000))

    def set_rapl_locked(self, locked):
        write(TEST_SYSFS + "/rapl_locked", int(bool(locked)))

    def add_switch(self, kind):
        """Create a "lid" or "tablet" switch input device (state 0)."""
        import evdev
        from evdev import ecodes

        code = {"lid": ecodes.SW_LID, "tablet": ecodes.SW_TABLET_MODE}[kind]
        if not os.path.exists("/dev/uinput"):
            sh("modprobe", "uinput")
        dev = evdev.UInput({ecodes.EV_SW: [code]},
                           name="thermald test %s switch" % kind)
        self.switches[kind] = (dev, code)
        time.sleep(0.2)           # let the event node appear

    def remove_switch(self, kind):
        dev, _ = self.switches.pop(kind)
        dev.close()

    def unload_module(self):
        """Remove the test module's zones, cooling devices and RAPL."""
        sh("rmmod", "thermald_test_kern_module")

    def load_module(self):
        """Load the test module again, like a driver that probes late."""
        self._insmod()
        for zone, celsius in self.platform.get("initial_temps", {}).items():
            self.set_temp(zone, celsius)

    def _set_switch(self, kind, value):
        from evdev import ecodes

        dev, code = self.switches[kind]
        dev.write(ecodes.EV_SW, code, int(bool(value)))
        dev.syn()

    def set_lid_closed(self, closed):
        self._set_switch("lid", closed)

    def set_tablet_mode(self, tablet):
        self._set_switch("tablet", tablet)

    def set_on_battery(self, on_battery):
        self._update_properties("org.freedesktop.UPower",
                                "/org/freedesktop/UPower",
                                "org.freedesktop.UPower",
                                {"OnBattery": bool(on_battery)})

    def set_power_profile(self, profile):
        """"power-saver", "balanced" or "performance"."""
        assert profile in POWER_PROFILES, profile
        self._update_properties("net.hadess.PowerProfiles",
                                "/net/hadess/PowerProfiles",
                                "net.hadess.PowerProfiles",
                                {"ActiveProfile": profile})

    # Observations

    def temp(self, zone):
        for line in read(TEST_SYSFS + "/zone_temp").splitlines():
            name, value = line.split()
            if name == zone:
                return int(value) / 1000
        raise KeyError(zone)

    def pl1(self, domain="mmio"):
        """PL1 of a fake RAPL domain in W."""
        return int(read(DOMAINS[domain] + "/constraint_0_power_limit_uw")) / 1e6

    def pl2(self, domain="mmio"):
        return int(read(DOMAINS[domain] + "/constraint_1_power_limit_uw")) / 1e6

    def rapl_enabled(self, domain="mmio"):
        return read(DOMAINS[domain] + "/enabled") == "1"

    def rapl_writes(self, domain=None, attr="constraint_0_power"):
        """RAPL writes so far: list of dicts (ms, domain, attr, value, ret)."""
        self._drain_rapl_log()
        return [w for w in self._rapl_log
                if (domain is None or w["domain"] == domain)
                and (attr is None or w["attr"] == attr)]

    def cpu_seconds(self):
        """CPU time thermald has used so far (user + system)."""
        with open("/proc/%d/stat" % self.proc.pid) as f:
            fields = f.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")

    def reinit(self):
        """thermald's D-Bus Reinit (needs "dbus": True and --dbus-enable)."""
        obj = self._bus().get_object("org.freedesktop.thermald",
                                     "/org/freedesktop/thermald")
        obj.Reinit(dbus_interface="org.freedesktop.thermald")

    def imok_values(self):
        """Arguments of every IETM.IMOK call so far (from dmesg)."""
        import re
        dmesg = sh("dmesg", check=False).stdout
        return [int(v) for v in re.findall(r"THDTST IMOK (\d+)", dmesg)]

    def log(self):
        self.log_file.flush()
        with open(self.log_path, errors="replace") as f:
            return f.read()

    def now(self):
        return time.monotonic() - self.t0

    # Waiting

    def wait_until(self, predicate, timeout, what):
        """Wait for predicate() to be true, fail with `what` on timeout.

        `what` can be a function, called on failure, so the message can
        show the state at the time of the failure.
        """
        def describe():
            return what() if callable(what) else what

        deadline = time.monotonic() + timeout
        while True:
            if predicate():
                return
            if not self.thermald_running() and self.proc is not None:
                raise ScenarioFailed("thermald exited (status %s) while "
                                     "waiting for: %s"
                                     % (self.proc.returncode, describe()))
            if time.monotonic() > deadline:
                raise ScenarioFailed("timeout after %ds waiting for: %s"
                                     % (timeout, describe()))
            time.sleep(0.25)

    def hold(self, predicate, seconds, what):
        """Fail as soon as predicate() is false during the next `seconds`."""
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if not predicate():
                raise ScenarioFailed(what() if callable(what) else what)
            if not self.thermald_running() and self.proc is not None:
                raise ScenarioFailed("thermald exited (status %s)"
                                     % self.proc.returncode)
            time.sleep(0.25)

    def wait_ready(self, timeout=30):
        """Wait until thermald's main loop runs (all startup paths log it)."""
        self.wait_log("Start main loop", timeout)

    def wait_log(self, text, timeout=30):
        self.wait_until(lambda: text in self.log(), timeout,
                        "log line containing %r" % text)

    def check(self, condition, message):
        if not condition:
            raise ScenarioFailed(message)

    # Internals

    def _insmod(self):
        p = self.platform
        args = []
        if p.get("zones"):
            args.append("zone_names=" + ",".join(p["zones"]))
        if p.get("cdevs"):
            args.append("cdev_names=" + ",".join(p["cdevs"]))
        rapl = p.get("rapl")
        if rapl is not None:
            args.append("rapl=1")
            for k in ("max_uw", "pl1_uw", "pl2_uw", "demand_uw",
                      "locked", "lock_errno"):
                if k in rapl:
                    args.append("rapl_%s=%s" % (k, int(rapl[k])))
        sh("insmod", os.path.join(self.repo, "test", "module",
                                  "thermald_test_kern_module.ko"), *args)

    def _start_dbus(self):
        conf = os.path.join(self.outdir, "dbus.conf")
        write(conf, DBUS_CONF)
        if os.path.exists(DBUS_SOCKET):
            os.unlink(DBUS_SOCKET)
        self.helpers.append(subprocess.Popen(
            ["dbus-daemon", "--config-file=" + conf, "--nofork",
             "--nopidfile"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        self.bus_address = "unix:path=" + DBUS_SOCKET
        deadline = time.monotonic() + 10
        while not os.path.exists(DBUS_SOCKET):
            if time.monotonic() > deadline:
                raise ScenarioFailed("private D-Bus daemon didn't start")
            time.sleep(0.05)

    def _bus(self):
        import dbus
        return dbus.bus.BusConnection(self.bus_address)

    def _start_mock(self, template, parameters, bus_name):
        env = dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=self.bus_address)
        log = open(os.path.join(self.outdir, template + ".log"), "w")
        self.helpers.append(subprocess.Popen(
            [sys.executable, "-m", "dbusmock", "--system", "--template",
             template, "-p", json.dumps(parameters)],
            stdout=log, stderr=subprocess.STDOUT, env=env))
        bus = self._bus()
        deadline = time.monotonic() + 15
        while not bus.name_has_owner(bus_name):
            if time.monotonic() > deadline:
                raise ScenarioFailed("dbusmock %s didn't start" % template)
            time.sleep(0.1)

    def _update_properties(self, bus_name, path, interface, properties):
        """Change mocked properties, emitting PropertiesChanged."""
        import dbus
        obj = self._bus().get_object(bus_name, path)
        obj.UpdateProperties(interface, properties,
                             dbus_interface="org.freedesktop.DBus.Mock")

    def _drain_rapl_log(self):
        with self._rapl_log_lock:
            self._drain_rapl_log_locked()

    def _drain_rapl_log_locked(self):
        while True:
            write(TEST_SYSFS + "/rapl_log", self._log_cursor)
            lines = read(TEST_SYSFS + "/rapl_log").splitlines()
            if not lines:
                return
            for line in lines:
                seq, ms, domain, attr, value, ret = line.split()
                self._rapl_log.append(dict(
                    seq=int(seq), ms=int(ms),
                    domain="mmio" if domain.endswith("mmio") else "msr",
                    attr=attr, value=int(value), ret=int(ret)))
                self._log_cursor = int(seq) + 1

    def _sample(self):
        sample = {"t": round(self.now(), 2)}
        for zone in self.platform.get("zones", []):
            sample[zone] = self.temp(zone)
        if self.platform.get("rapl") is not None:
            for d in DOMAINS:
                sample["pl1_" + d] = self.pl1(d)
        return sample

    def _start_sampling(self):
        if self._sampling:
            return
        self._sampling = True

        def loop():
            while self._sampling:
                try:
                    self.trace.append(self._sample())
                    if self.platform.get("rapl") is not None:
                        self._drain_rapl_log()
                except OSError:
                    pass
                time.sleep(0.5)

        self._sampler = threading.Thread(target=loop, daemon=True)
        self._sampler.start()

    def finish(self):
        self._sampling = False
        if self.platform.get("rapl") is not None:
            try:
                self._drain_rapl_log()
            except OSError:
                pass
        if self.thermald_running():
            try:
                self.stop_thermald()
            except ScenarioFailed:
                pass
        for helper in self.helpers:
            helper.terminate()
        if os.path.exists(self.log_path):
            shutil.copy(self.log_path, os.path.join(self.outdir, "thermald.log"))
        for dev, _ in self.switches.values():
            dev.close()


def load_scenario(path):
    spec = importlib.util.spec_from_file_location("scenario", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    repo, scenario_path, outdir = sys.argv[1:4]
    os.makedirs(outdir, exist_ok=True)
    scenario = load_scenario(scenario_path)
    platform = scenario.PLATFORM
    platform_json = os.path.join(outdir, "platform.json")
    if os.path.exists(platform_json):
        with open(platform_json) as f:
            platform = json.load(f)
    g = Guest(repo, platform, outdir)
    result = {"scenario": os.path.basename(scenario_path), "passed": False}
    start = time.monotonic()

    try:
        g.setup()
        if platform.get("autostart", True):
            g.start_thermald()
        scenario.run(g)
        result["passed"] = True
    except ScenarioFailed as e:
        result["error"] = str(e)
    except Exception:
        result["error"] = traceback.format_exc()
    finally:
        g.finish()
        result["duration_s"] = round(time.monotonic() - start, 1)
        result["trace"] = g.trace
        result["rapl_writes"] = g._rapl_log
        with open(os.path.join(outdir, "dmesg.log"), "w") as f:
            f.write(sh("dmesg", check=False).stdout)
        with open(os.path.join(outdir, "result.json"), "w") as f:
            json.dump(result, f, indent=1)

    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
