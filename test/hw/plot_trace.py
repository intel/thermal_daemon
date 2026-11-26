#!/usr/bin/python3
"""Plot temperature and power from a hardware smoke test report.

    test/hw/plot_trace.py /tmp/thermald-hw/<host>-<time>/report.json

Writes plot.png and trace.csv next to the report. hw_smoke.py calls this
at the end of every run. Needs python3-matplotlib.

Two charts on one time axis (temperature and power have different
scales, so they don't share an axis):

- top: the sensors thermald uses to limit power, i.e. the zones in its
  last zone dump (thermald.log next to the report) with a passive trip on
  a RAPL cooling device, each with its trip temperatures as thin lines;
  then the CPU package temperature, then the hottest other sensor, up to
  three lines
- bottom: package power and the PL1 limit

The CPU load phase is shaded in both.
"""

import csv
import json
import os
import re
import sys

# Reference palette (light): surface, text and line tokens, and the
# first three categorical slots, validated all-pairs (CVD and normal
# vision). Aqua is below 3:1 on the surface, so every line is also
# labelled directly and trace.csv carries the values.
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASELINE = "#c3c2b7"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]
LOAD_BAND = "#f0efec"

CPU_SENSORS = ("x86_pkg_temp", "TCPU", "B0D4")


def power_trips(log_path):
    """{zone: [trip temperatures in C]} for passive trips on a RAPL
    cooling device, from the last ZONE DUMP in thermald's log."""
    try:
        with open(log_path, errors="replace") as f:
            text = f.read()
    except OSError:
        return {}
    start = text.rfind("ZONE DUMP BEGIN")
    if start < 0:
        return {}
    dump = text[start:text.find("ZONE DUMP END", start)]
    trips, zone, temp = {}, None, None
    for line in dump.splitlines():
        m = re.search(r"Zone \d+: (\S+), Active:1", line)
        if m:
            zone, temp = m.group(1), None
            continue
        m = re.search(r"type:passive temp:(-?\d+)", line)
        if m:
            temp = int(m.group(1)) / 1000
            continue
        if zone and temp is not None and re.search(r"cdev\[\d+\] rapl", line):
            trips.setdefault(zone, [])
            if temp not in trips[zone]:
                trips[zone].append(temp)
    return trips


def pick_temperatures(trace, trips):
    """Up to three sensors: those with power trips, the CPU package
    temperature, then the hottest other one. Order within each group is
    by name, so a sensor keeps its color from run to run."""
    names = set()
    for s in trace:
        names.update(s.get("temps", {}))
    picked = sorted(n for n in trips if n in names)
    cpu = next((n for n in CPU_SENSORS if n in names), None)
    if cpu and cpu not in picked:
        picked.append(cpu)
    peak = {n: max((s["temps"].get(n, -273) for s in trace), default=-273)
            for n in names}
    rest = [n for n in names if n not in picked and not n.startswith("INT3400")
            and n not in CPU_SENSORS]
    picked += sorted(rest, key=lambda n: (-peak[n], n))[:max(0, 3 - len(picked))]
    return picked[:3]


def label_x(trace, level, sensors, width=None):
    """Time where a label above the line at `level` collides with the
    fewest samples of the plotted `sensors` (those within -1..+5 C of
    `level`), over about a label's width (18% of the run)."""
    if not trace:
        return 0
    if width is None:
        width = max(12, int(len(trace) * 0.18))
    hits = [sum(1 for n in sensors
                if level - 1 <= s.get("temps", {}).get(n, -273) <= level + 5)
            for s in trace]
    best, best_i = None, 0
    for i in range(max(1, len(trace) - width)):
        cost = sum(hits[i:i + width])
        if best is None or cost < best:
            best, best_i = cost, i
    return trace[best_i]["t"]


def write_csv(trace, sensors, path):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "package_power_w", "pl1_w"] + ["%s_c" % n for n in sensors])
        for s in trace:
            w.writerow([s["t"], s.get("pkg_power_w", ""), s.get("pl1_w", "")]
                       + [s.get("temps", {}).get(n, "") for n in sensors])


def end_labels(ax, lines, span):
    """Label each line at its end; skip a label that would sit on top of
    one already placed (the legend still names it)."""
    placed = []
    for label, xs, ys, color in sorted(lines, key=lambda l: -l[2][-1]):
        if not ys:
            continue
        y = ys[-1]
        ax.plot([xs[-1]], [y], "o", markersize=8, color=color,
                markeredgecolor=SURFACE, markeredgewidth=2, zorder=4)
        if any(abs(y - p) < 0.06 * span for p in placed):
            continue
        placed.append(y)
        ax.annotate("%s %.0f" % (label, y), (xs[-1], y), xytext=(8, 0),
                    textcoords="offset points", va="center", fontsize=9,
                    color=TEXT_PRIMARY)


def style(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(BASELINE)
    ax.tick_params(colors=MUTED, labelcolor=TEXT_SECONDARY, labelsize=9)
    ax.grid(axis="y", color=GRID, linewidth=1, linestyle="-")
    ax.set_axisbelow(True)


def plot(report_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    with open(report_path) as f:
        report = json.load(f)
    trace = report.get("trace", [])
    out = os.path.dirname(os.path.abspath(report_path))
    trips = power_trips(os.path.join(out, "thermald.log"))
    sensors = pick_temperatures(trace, trips)
    write_csv(trace, sensors, os.path.join(out, "trace.csv"))
    if not trace:
        return None

    t = [s["t"] for s in trace]
    fig, (top, bottom) = plt.subplots(
        2, 1, sharex=True, figsize=(10, 6.5), dpi=150,
        gridspec_kw={"height_ratios": [1, 1], "hspace": 0.32})
    fig.patch.set_facecolor(SURFACE)
    plt.rcParams["lines.solid_capstyle"] = "round"
    plt.rcParams["lines.solid_joinstyle"] = "round"

    load = report.get("load_s")
    for ax in (top, bottom):
        style(ax)
        if load:
            ax.axvspan(load[0], load[1], color=LOAD_BAND, zorder=0, linewidth=0)

    # Temperatures
    lines, lo, hi = [], None, None
    for i, name in enumerate(sensors):
        xs = [s["t"] for s in trace if name in s.get("temps", {})]
        ys = [s["temps"][name] for s in trace if name in s.get("temps", {})]
        if not ys:
            continue
        top.plot(xs, ys, color=SERIES[i], linewidth=2, label=name)
        # Its trip temperatures: thin, in the sensor's color, with one
        # label for all of them above the highest, where no other line
        # is close to it
        sensor_trips = sorted(trips.get(name, []))
        for trip in sensor_trips:
            top.axhline(trip, color=SERIES[i], linewidth=1, alpha=0.6, zorder=1)
        if sensor_trips:
            top_trip = sensor_trips[-1]
            top.annotate("%s trip%s %s °C" % (
                             name, "s" if len(sensor_trips) > 1 else "",
                             ", ".join("%g" % t for t in sensor_trips)),
                         (label_x(trace, top_trip, sensors), top_trip), xytext=(4, 3),
                         textcoords="offset points", fontsize=8,
                         color=TEXT_SECONDARY)
        lines.append((name, xs, ys, SERIES[i]))
        span = ys + trips.get(name, [])     # keep trip lines in view
        lo = min(span + ([lo] if lo is not None else []))
        hi = max(span + ([hi] if hi is not None else []))
    if lines:
        end_labels(top, lines, (hi - lo) or 1)
    top.set_ylabel("Temperature (°C)", color=TEXT_SECONDARY, fontsize=10)

    # Power: measured package power and the PL1 limit
    power = [(s["t"], s["pkg_power_w"]) for s in trace
             if s.get("pkg_power_w") is not None]
    pl1 = [(s["t"], s["pl1_w"]) for s in trace if s.get("pl1_w") is not None]
    plines = []
    if power:
        xs, ys = zip(*power)
        bottom.plot(xs, ys, color=SERIES[0], linewidth=2, label="package power")
        plines.append(("package", list(xs), list(ys), SERIES[0]))
    if pl1:
        xs, ys = zip(*pl1)
        bottom.plot(xs, ys, color=SERIES[1], linewidth=2, drawstyle="steps-post",
                    label="PL1 limit")
        plines.append(("PL1", list(xs), list(ys), SERIES[1]))
    allp = [y for _, _, ys, _ in plines for y in ys]
    if allp:
        bottom.set_ylim(0, max(allp) * 1.12)
        end_labels(bottom, plines, max(allp) or 1)
    bottom.set_ylabel("Power (W)", color=TEXT_SECONDARY, fontsize=10)
    bottom.set_xlabel("Time (s)", color=TEXT_SECONDARY, fontsize=10)
    bottom.set_xlim(0, max(t) * 1.06 if max(t) else 1)

    for ax in (top, bottom):
        if len(ax.get_lines()) > 1:
            # Above the plot, where it can't cover a line
            leg = ax.legend(loc="lower left", bbox_to_anchor=(0, 1.0),
                            frameon=False, fontsize=9, ncol=3,
                            handlelength=1.6, borderaxespad=0.2)
            for text in leg.get_texts():
                text.set_color(TEXT_PRIMARY)
    if load:
        # In the power chart's headroom (its y range ends 12% above the
        # highest value), so it doesn't sit on a temperature line
        bottom.annotate("CPU load", ((load[0] + load[1]) / 2, 0.97),
                        xycoords=("data", "axes fraction"), ha="center",
                        va="top", fontsize=9, color=TEXT_SECONDARY)

    result = report.get("result", "").upper()
    fig.suptitle("thermald hardware smoke test — %s" % report.get("host", ""),
                 x=0.06, ha="left", fontsize=12, color=TEXT_PRIMARY)
    fig.text(0.06, 0.935, "%s · %s" % (result, " ".join(report.get(
             "thermald_args", []))), fontsize=8.5, color=TEXT_SECONDARY)
    fig.subplots_adjust(left=0.07, right=0.9, top=0.85, bottom=0.08)

    path = os.path.join(out, "plot.png")
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)
    return path


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        sys.exit(2)
    print(plot(sys.argv[1]))
