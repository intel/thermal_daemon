#!/usr/bin/python3
"""Write a Markdown test report from the results of test/run_all.sh.

    test/report.py DIR [--hw /tmp/thermald-hw/<host>-<time>/report.json ...]

DIR is the --report directory of run_all.sh, holding unit.xml and vm.xml
(pytest JUnit XML), fuzz.log, status.txt and vault_count.txt. Writes
DIR/report.md; hardware smoke test reports given with --hw are included,
with their plots copied next to the report.

Private data vaults are only counted: their names and contents are not
written to the report.
"""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def sh(*cmd, cwd=None):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd,
                              timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def read(path):
    try:
        with open(path, errors="replace") as f:
            return f.read().strip()
    except OSError:
        return ""


def os_name():
    for line in read("/etc/os-release").splitlines():
        if line.startswith("PRETTY_NAME="):
            return line.split("=", 1)[1].strip('"')
    return platform.system()


def cpu_model():
    for line in read("/proc/cpuinfo").splitlines():
        if line.startswith("model name"):
            return line.split(":", 1)[1].strip()
    return platform.processor()


def environment(vm_kernel, tested):
    """tested: branch, commit and a changed-file line recorded by
    run_all.sh when the tests started (commit.txt), else the tree now."""
    if tested:
        branch, commit = tested[0], tested[1]
        dirty = len(tested) > 2 and tested[2]
    else:
        branch = sh("git", "rev-parse", "--abbrev-ref", "HEAD", cwd=REPO)
        commit = sh("git", "log", "-1", "--format=%h %s", cwd=REPO)
        dirty = sh("git", "status", "--porcelain", "--untracked-files=no",
                   cwd=REPO)
    return [
        ("Date", datetime.datetime.now().strftime("%Y-%m-%d %H:%M %Z").strip()),
        ("Machine", "%s %s" % (read("/sys/class/dmi/id/sys_vendor"),
                               read("/sys/class/dmi/id/product_name"))),
        ("CPU", cpu_model()),
        ("OS", os_name()),
        ("Kernel", platform.release()),
        ("thermald", "%s, %s%s" % (branch, commit,
                                   " (with uncommitted changes)" if dirty else "")),
        ("Compilers", "%s; %s" % (sh("gcc", "--version").split("\n")[0],
                                  sh("clang", "--version").split("\n")[0] or "no clang")),
        ("VM kernel", vm_kernel or platform.release()),
        ("virtme-ng", sh("vng", "--version") or "not installed"),
    ]


def junit(path):
    """[(name, result, seconds, message)] from a pytest JUnit XML file."""
    if not os.path.exists(path):
        return None
    cases = []
    for case in ET.parse(path).getroot().iter("testcase"):
        name = case.get("name", "")
        # VM scenarios and real vaults by their own name; other
        # parametrized tests keep their full name
        m = re.match(r"test_scenario\[(.+)\]$", name)
        if m:
            name = m.group(1)
        m = re.match(r"test_real_vault\[(.+)\]$", name)
        if m:
            name = "real vault %d" % (len([c for c in cases
                                           if c[0].startswith("real vault")]) + 1)
        result, message = "PASS", ""
        for tag, res in (("failure", "FAIL"), ("error", "ERROR"),
                         ("skipped", "SKIP")):
            el = case.find(tag)
            if el is not None:
                result = res
                message = (el.get("message") or "").strip()
                if tag == "skipped" and "xfail" in (el.get("type") or "") + message:
                    result = "XFAIL"
                break
        cases.append((name, result, float(case.get("time", 0)), message))
    return cases


FUZZ_TARGETS = {
    "gddv": "data vault (GDDV) parser: `cthd_gddv::parse_gddv()` and "
            "`merge_appc()` on arbitrary bytes, as `gddv_init()` runs them "
            "on the INT3400 `data_vault`: header checks, nested segments, "
            "LZMA decompression, key/value and integer/string readers, and "
            "the table parsers (PSVT, PPCC, APCT, APAT, ITMT, VSCT/VSPT, "
            "IDSP, trip points). Seeds: one vault per table type plus any "
            "private vaults",
    "xml": "XML configuration parsers: thermal-conf.xml, "
           "thermald-features.xml and thermal-cpu-cdev-order.xml, each "
           "read by the real `parser_init()` from a file (file checks, DTD "
           "rejection, libxml2 options as in the daemon), then parsed, "
           "dumped, matched to the platform and read back. Seeds: the "
           "shipped and test configurations, with a dictionary of the "
           "element names",
    "util": "string helpers that see outside input: `parse_int_value()`, "
            "`parse_double_value()` (range and NaN checks asserted) and "
            "`is_valid_thermal_object_name()` (no '/' accepted)",
}


def fuzz_one(text):
    runs = re.search(r"Done (\d+) runs in (\d+) second", text)
    cov = re.findall(r"cov: (\d+) ft: (\d+) corp: (\d+)", text)
    crash = re.search(r"SUMMARY: (\S+): (.+)", text)
    not_run = re.search(r"=== fuzz target \S+: not run", text)
    return {
        "runs": int(runs.group(1)) if runs else None,
        "seconds": int(runs.group(2)) if runs else None,
        "coverage": int(cov[-1][0]) if cov else None,
        "corpus": int(cov[-1][2]) if cov else None,
        "finding": "%s: %s" % (crash.group(1), crash.group(2)) if crash
                   else ("crash" if "FOUND A CRASH" in text else None),
        "not_run": bool(not_run),
    }


def fuzz_summary(path):
    """{target: summary} from run.sh's output, in run order."""
    text = read(path)
    if not text:
        return None
    parts = re.split(r"^=== fuzz target: (\S+) ===$", text, flags=re.M)
    if len(parts) == 1:             # older run.sh: the GDDV target only
        return {"gddv": fuzz_one(text)}
    return {parts[i]: fuzz_one(parts[i + 1]) for i in range(1, len(parts), 2)}


def fuzz_result(f):
    if f["not_run"]:
        return "not run"
    if f["finding"]:
        return "finding: " + f["finding"]
    return "%s inputs in %s s, coverage %s edges, no findings" % (
        f["runs"], f["seconds"], f["coverage"])


WORDS = {"PASS": "passed", "FAIL": "failed", "ERROR": "errors",
         "SKIP": "skipped", "XFAIL": "xfailed"}


def tally(cases):
    c = {}
    for _, r, _, _ in cases:
        c[r] = c.get(r, 0) + 1
    order = ["PASS", "FAIL", "ERROR", "XFAIL", "SKIP"]
    return ", ".join("%d %s" % (c[r], WORDS[r]) for r in order if r in c)


def cell(text):
    return str(text).replace("|", "\\|").replace("\n", " ")


def write_cases(out, title, cases):
    out.append("## %s\n" % title)
    if cases is None:
        out.append("Not run.\n")
        return
    out.append("%s (%d tests).\n" % (tally(cases), len(cases)))
    out.append("| Test | Result | Time |")
    out.append("|---|---|---|")
    for name, result, secs, message in cases:
        note = " — %s" % cell(message[:160]) if message and result != "PASS" else ""
        out.append("| `%s` | %s%s | %.1f s |" % (cell(name), result, note, secs))
    out.append("")


def write_hw(out, reports, outdir):
    out.append("## Hardware smoke test\n")
    if not reports:
        out.append("Not included (it runs separately, as root, on a real "
                   "machine: `sudo test/hw/hw_smoke.py`).\n")
        return
    for i, path in enumerate(reports, 1):
        with open(path) as f:
            r = json.load(f)
        out.append("### %s: %s\n" % (r.get("host", "?"), r.get("result", "?").upper()))
        rows = [("thermald arguments", " ".join(r.get("thermald_args", []))),
                ("PPCC range", "%s–%s W" % tuple(r["ppcc_w"]) if r.get("ppcc_w") else "-"),
                ("Exit status", r.get("exit_status")),
                ("Failures", "; ".join(r.get("failures", [])) or "none"),
                ("Notes", "; ".join(r.get("notes", [])) or "none")]
        if r.get("load_s"):
            rows.insert(1, ("CPU load", "%g–%g s of the run" % tuple(r["load_s"])))
        out.append("| | |")
        out.append("|---|---|")
        for k, v in rows:
            out.append("| %s | %s |" % (k, cell(v)))
        out.append("")
        plot = os.path.join(os.path.dirname(path), "plot.png")
        if os.path.exists(plot):
            name = "hw_plot_%d.png" % i
            shutil.copy(plot, os.path.join(outdir, name))
            out.append("![temperature and power over the run](%s)\n" % name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dir")
    ap.add_argument("--hw", action="append", default=[],
                    help="a hardware smoke test report.json to include")
    args = ap.parse_args()
    d = args.dir

    status = dict(line.split(": ", 1) for line in read(os.path.join(d, "status.txt")).splitlines()
                  if ": " in line)
    vaults = read(os.path.join(d, "vault_count.txt")) or "0"
    unit = junit(os.path.join(d, "unit.xml"))
    vm = junit(os.path.join(d, "vm.xml"))
    fuzz = fuzz_summary(os.path.join(d, "fuzz.log"))

    out = ["# thermald test report\n"]
    out.append("Generated by `test/run_all.sh --report`; see `test/README.md` "
               "for what each test checks.\n")
    out.append("## Environment\n")
    out.append("| | |")
    out.append("|---|---|")
    tested = read(os.path.join(d, "commit.txt")).splitlines()
    for k, v in environment(read(os.path.join(d, "vm_kernel.txt")),
                            tested if len(tested) >= 2 else None):
        out.append("| %s | %s |" % (k, cell(v)))
    out.append("| Private data vaults | %s (not included in this report) |" % vaults)
    out.append("")

    out.append("## Summary\n")
    out.append("| Layer | Result | Details |")
    out.append("|---|---|---|")

    def counts(cases):
        return "not run" if cases is None else tally(cases)
    out.append("| Parser unit tests | %s | %s |" % (status.get("unit", "not run"), counts(unit)))
    if fuzz:
        details = "; ".join("%s: %s" % (t, fuzz_result(f)) for t, f in fuzz.items())
    else:
        details = "not run"
    out.append("| Fuzzing | %s | %s |" % (status.get("fuzz", "not run"), cell(details)))
    out.append("| VM regression | %s | %s |" % (status.get("vm", "not run"), counts(vm)))
    hw = []
    for path in args.hw:
        with open(path) as f:
            hw.append(json.load(f).get("result", "?").upper())
    out.append("| Hardware smoke | %s | %s |" % (", ".join(hw) or "not included",
               "%d run(s)" % len(hw) if hw else "run separately as root"))
    out.append("")

    write_cases(out, "Parser unit tests", unit)
    out.append("## Fuzzing\n")
    if fuzz:
        out.append("libFuzzer, built with ASan and UBSan (alignment check "
                   "off: the data vault only exists on x86). D-Bus input is "
                   "fuzzed by the VM scenarios `dbus_fuzz_adaptive` and "
                   "`dbus_fuzz_default`: random calls to every method, "
                   "valid and invalid arguments, against an ASan build of "
                   "the running daemon. Not fuzzed: sysfs input.\n")
        out.append("| Target | Inputs | Time | Coverage | Corpus | Result |")
        out.append("|---|---|---|---|---|---|")
        for t, f in fuzz.items():
            result = ("not run" if f["not_run"] else
                      "finding: `%s`" % f["finding"] if f["finding"] else "no findings")
            out.append("| `%s` | %s | %s s | %s edges | %s | %s |" % (
                t, f["runs"], f["seconds"], f["coverage"], f["corpus"],
                cell(result)))
        out.append("")
        for t in fuzz:
            if t in FUZZ_TARGETS:
                out.append("- `%s`: %s." % (t, FUZZ_TARGETS[t]))
        out.append("")
    else:
        out.append("Not run.\n")
    write_cases(out, "VM regression scenarios", vm)
    write_hw(out, args.hw, d)

    path = os.path.join(d, "report.md")
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")
    print(path)


if __name__ == "__main__":
    main()
