#!/usr/bin/python3
"""Hardware smoke test: run this tree's thermald on a real machine.

    sudo test/hw/hw_smoke.py [--seconds 60] [--load 30] [--no-adaptive]
                             [--cpuid-check] [--loglevel info|debug]

Checks, on the real platform (its own data vault, sensors and RAPL):
  - thermald starts and keeps running (in adaptive mode unless
    --no-adaptive), with no missing zones
  - every PL1 it writes is within the PPCC range it logged
  - after SIGTERM every powercap limit, time window and enable bit is
    back at the value it had before thermald started

thermald runs with --ignore-cpuid-check, so that platforms thermald
would otherwise refuse can be tested too: pre-production CPU models not
in its list yet, and Lenovo ThinkPads with DYTC (thinkpad_acpi
dytc_lapmode), where the firmware normally owns thermal policy.
--cpuid-check keeps thermald's normal platform check, to test that
check itself.

thermald's log (--loglevel=info by default, enough for these checks)
is kept as thermald.log next to the report; use --loglevel debug for
more detail. plot.png (temperatures, package power and PL1 over time)
and trace.csv are written there too; the plot needs python3-matplotlib.

--load runs a busy loop on every CPU for that many seconds in the
middle of the run, so thermald has something to control.

Stop the thermald service first; this script won't. A report, the
thermald log and a trace go to /tmp/thermald-hw/<host>-<time>/.
Exit status: 0 pass, 1 fail, 2 can't run (not root, service running,
platform refused by thermald).
"""

import argparse
import glob
import json
import multiprocessing
import os
import re
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
POWERCAP = "/sys/class/powercap"
IGNORE_ADAPTIVE = "/var/run/thermald/ignore_adaptive"


def read(path):
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        return None


def powercap_state():
    """Every writable powercap setting: {path: value}."""
    state = {}
    for zone in glob.glob(POWERCAP + "/*:*"):
        for attr in glob.glob(zone + "/constraint_*_power_limit_uw") \
                + glob.glob(zone + "/constraint_*_time_window_us") \
                + [zone + "/enabled"]:
            value = read(attr)
            if value is not None:
                state[os.path.realpath(attr)] = value
    return state


def mmio_pl1():
    for path in glob.glob(POWERCAP + "/intel-rapl-mmio:0/constraint_0_power_limit_uw") \
            + glob.glob(POWERCAP + "/intel-rapl:0/constraint_0_power_limit_uw"):
        value = read(path)
        if value is not None:
            return int(value) / 1e6
    return None


class PackagePower:
    """Package power in W from the RAPL energy counter, between calls."""

    def __init__(self):
        self.path = None
        for zone in sorted(glob.glob(POWERCAP + "/intel-rapl:*")) \
                + sorted(glob.glob(POWERCAP + "/intel-rapl-mmio:*")):
            if read(zone + "/name") == "package-0" and \
                    read(zone + "/energy_uj") is not None:
                self.path = zone
                break
        self.range = int(read(self.path + "/max_energy_range_uj") or 0) \
            if self.path else 0
        self.last = None

    def sample(self):
        if not self.path:
            return None
        now, energy = time.monotonic(), int(read(self.path + "/energy_uj"))
        last, self.last = self.last, (now, energy)
        if not last or now - last[0] < 0.1:     # too short to measure
            return None
        delta = energy - last[1]
        if delta < 0:                   # counter wrapped
            delta += self.range
        return round(delta / (now - last[0]) / 1e6, 2)


def zone_temps():
    temps = {}
    for zone in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(zone + "/temp")
        if t is not None and t.lstrip("-").isdigit():
            temps[read(zone + "/type")] = int(t) / 1000
    return temps


def busy(stop_at):
    while time.time() < stop_at:
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--load", type=int, default=0,
                    help="seconds of full CPU load in the middle")
    ap.add_argument("--no-adaptive", action="store_true")
    ap.add_argument("--cpuid-check", action="store_true",
                    help="keep thermald's CPU model and platform check "
                    "(default: --ignore-cpuid-check)")
    ap.add_argument("--loglevel", choices=("info", "debug"), default="info",
                    help="thermald log level (default: info)")
    ap.add_argument("--thermald", default=os.path.join(REPO, "thermald"))
    args = ap.parse_args()

    if os.geteuid() != 0:
        print("run as root", file=sys.stderr)
        return 2
    if subprocess.run(["systemctl", "is-active", "--quiet", "thermald"]).returncode == 0:
        print("the thermald service is running: stop it first "
              "(systemctl stop thermald)", file=sys.stderr)
        return 2

    out = "/tmp/thermald-hw/%s-%s" % (socket.gethostname().split(".")[0],
                                      time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out)
    report = {"host": socket.gethostname(), "failures": [], "notes": []}
    fail = report["failures"].append
    note = report["notes"].append

    if os.path.exists(IGNORE_ADAPTIVE):
        note("%s exists from an earlier run: thermald will skip adaptive "
             "mode until it is removed" % IGNORE_ADAPTIVE)

    before = powercap_state()
    cmd = [args.thermald, "--no-daemon", "--loglevel=" + args.loglevel,
           "--poll-interval=1"]
    if not args.no_adaptive:
        cmd.append("--adaptive")
    if not args.cpuid_check:
        cmd.append("--ignore-cpuid-check")
    report["thermald_args"] = cmd[1:]
    log_path = os.path.join(out, "thermald.log")
    log = open(log_path, "w")
    start = time.time()
    proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)

    trace = []
    package = PackagePower()
    package.sample()
    load_from = start + (args.seconds - args.load) / 2
    workers = []
    while time.time() - start < args.seconds and proc.poll() is None:
        now = time.time()
        if args.load and not workers and now >= load_from:
            report["load_s"] = [round(now - start, 1),
                                round(now - start + args.load, 1)]
            workers = [multiprocessing.Process(target=busy,
                                               args=(now + args.load,))
                       for _ in range(os.cpu_count())]
            for w in workers:
                w.start()
        trace.append({"t": round(now - start, 1), "pl1_w": mmio_pl1(),
                      "pkg_power_w": package.sample(),
                      "temps": zone_temps()})
        time.sleep(0.5)
    for w in workers:
        w.terminate()

    exited_early = proc.poll() is not None
    if not exited_early:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(15)
        except subprocess.TimeoutExpired:
            proc.kill()
            fail("thermald didn't exit 15 s after SIGTERM")
    time.sleep(1)
    after = powercap_state()
    log.close()

    with open(log_path, errors="replace") as f:
        text = f.read()

    report["exit_status"] = proc.returncode
    if exited_early:
        if "Thermald can't run on this platform" in text \
                or "Unsupported cpu model" in text:
            note("thermald refuses this platform"
                 + ("" if args.cpuid_check else
                    " (even with --ignore-cpuid-check)"))
            report["result"] = "skip"
        else:
            fail("thermald exited by itself after %.0f s (status %s)"
                 % (time.time() - start, proc.returncode))
    elif proc.returncode != 0:
        fail("exit status %s after SIGTERM" % proc.returncode)

    if not args.no_adaptive:
        for bad, what in (("--adaptive option failed", "fell back to non-adaptive mode"),
                          ("Unable to find a zone", "a PSVT target has no zone"),
                          ("couldn't create any zones", "no adaptive zones")):
            if bad in text:
                fail(what)
    for m in re.finditer(r"Unsupported condition \d+ \((\w+)\)", text):
        note("unsupported condition %s" % m.group(1))
    if "Falling back to use configuration with the highest power" in text:
        note("no condition set can match: fallback target")

    ppcc = re.search(r"ppcc limits max:(\d+) min:(\d+)", text)
    if ppcc:
        hi, lo = int(ppcc.group(1)) / 1e6, int(ppcc.group(2)) / 1e6
        report["ppcc_w"] = [lo, hi]
        for m in re.finditer(r"set cdev state index \d+ state -?\d+ wr:(\d+)", text):
            w = int(m.group(1)) / 1e6
            if not lo <= w <= hi and w < 100:     # 200 W parks the MSR
                fail("PL1 %.1f W written outside the PPCC range %g-%g W"
                     % (w, lo, hi))
                break

    for path, value in before.items():
        if after.get(path) != value:
            fail("%s: %s before, %s after exit" % (path, value, after.get(path)))

    report["notes"] = sorted(set(report["notes"]))
    report.setdefault("result", "fail" if report["failures"] else "pass")
    with open(os.path.join(out, "report.json"), "w") as f:
        json.dump(dict(report, trace=trace, powercap_before=before,
                       powercap_after=after), f, indent=1)

    print("%s: %s" % (report["host"], report["result"].upper()))
    for line in report["failures"]:
        print("  FAIL", line)
    for line in report["notes"]:
        print("  note", line)
    plot_path = None
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import plot_trace
        plot_path = plot_trace.plot(os.path.join(out, "report.json"))
    except ImportError as e:
        print("  no plot (%s): install python3-matplotlib" % e)

    print("  thermald log:", log_path)
    print("  report:", os.path.join(out, "report.json"))
    if plot_path:
        print("  plot:", plot_path, "(data: trace.csv)")
    return {"pass": 0, "fail": 1, "skip": 2}[report["result"]]


if __name__ == "__main__":
    sys.exit(main())
