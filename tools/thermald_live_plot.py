#!/usr/bin/python3
"""Live plot of package power, PL1/PL2 and thermal zone temperatures.

    sudo -E tools/thermald_live_plot.py
    sudo -E tools/thermald_live_plot.py --zones TCPU,SEN2 --window 600
    sudo tools/thermald_live_plot.py --no-plot --csv /tmp/trace.csv

Every --interval seconds it reads:

- package power, from the RAPL energy counter (energy_uj) of the MMIO
  domain (/sys/class/powercap/intel-rapl-mmio:0), or of the MSR domain
  (intel-rapl:0) if MMIO has no counter or with --domain msr
- PL1 and PL2 (constraint_0/1_power_limit_uw) of the same domain
- the temperature of each thermal zone (/sys/class/thermal)

and updates a window with two charts on one time axis: temperatures on
top, power and the limits below (they have different units, so they
don't share an axis). Each line is labelled at its end with its current
value. A line per sample is also printed (--quiet to stop it), and
--csv keeps every sample.

energy_uj is readable by root only, so run it with sudo; -E keeps
DISPLAY and the session so the window can open. Without a display, use
--no-plot, or --save FILE to rewrite a PNG at every update. Needs
python3-matplotlib for the plot.

It only reads sysfs; it doesn't change any limit and can run while
thermald runs. Note that thermald parks the MSR PL1 at 200 W when it
controls PL1 through MMIO, so the MMIO domain shows the limit in use.
"""

import argparse
import csv
import glob
import os
import sys
import time

POWERCAP = "/sys/class/powercap"
THERMAL = "/sys/class/thermal"

# Reference palette (light), the validated categorical order. Colors
# follow the zone (its position in the list given or found), not its
# rank; at most eight zones, the size of the palette.
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASELINE = "#c3c2b7"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100",
          "#e87ba4", "#008300", "#6250d6", "#e34948"]
MAX_ZONES = len(SERIES)

# Listed first by default, so a long zone list keeps them
CPU_ZONES = ("x86_pkg_temp", "TCPU", "TCPU_PCI", "B0D4")


def read(path):
    with open(path) as f:
        return f.read().strip()


def read_int(path):
    try:
        return int(read(path))
    except (OSError, ValueError):
        return None


class Rapl:
    """Package power from energy_uj, and PL1/PL2, of one powercap domain."""

    def __init__(self, which):
        mmio = os.path.join(POWERCAP, "intel-rapl-mmio:0")
        msr = os.path.join(POWERCAP, "intel-rapl:0")
        candidates = {"mmio": [mmio], "msr": [msr], "auto": [mmio, msr]}[which]
        self.path = next((p for p in candidates if os.path.isdir(p)), None)
        if not self.path:
            sys.exit("No RAPL powercap domain (%s)" % ", ".join(candidates))
        # MMIO may have limits but no energy counter: take power from MSR
        self.energy_path = self.path
        if (which == "auto" and not os.path.exists(os.path.join(self.path, "energy_uj"))
                and os.path.isdir(msr)):
            self.energy_path = msr
        self.max_range = read_int(os.path.join(self.energy_path, "max_energy_range_uj"))
        self.last = None
        self.readable = os.access(os.path.join(self.energy_path, "energy_uj"), os.R_OK)
        if not self.readable:
            print("%s/energy_uj is not readable: run with sudo for package power"
                  % self.energy_path, file=sys.stderr)

    def name(self):
        if self.energy_path != self.path:
            return "%s (power from %s)" % (os.path.basename(self.path),
                                           os.path.basename(self.energy_path))
        return os.path.basename(self.path)

    def power_w(self):
        """Average power since the previous call, None on the first one."""
        if not self.readable:
            return None
        energy = read_int(os.path.join(self.energy_path, "energy_uj"))
        now = time.monotonic()
        if energy is None:
            return None
        watts = None
        if self.last:
            delta = energy - self.last[0]
            if delta < 0 and self.max_range:        # the counter wrapped
                delta += self.max_range
            dt = now - self.last[1]
            if dt > 0 and delta >= 0:
                watts = delta / dt / 1e6
        self.last = (energy, now)
        return watts

    def limit_w(self, constraint):
        uw = read_int(os.path.join(self.path, "constraint_%d_power_limit_uw" % constraint))
        return uw / 1e6 if uw is not None else None


def find_zones(wanted):
    """[(type, temp path)]: in the order of --zones, else the CPU
    temperatures first and then the other zones in thermal_zone order.
    INT3400 reports a constant and is skipped by default."""
    zones = []
    for d in sorted(glob.glob(os.path.join(THERMAL, "thermal_zone*")),
                    key=lambda p: int(p.rsplit("zone", 1)[1])):
        try:
            zones.append((read(os.path.join(d, "type")), os.path.join(d, "temp")))
        except OSError:
            continue
    if wanted:
        by_type = dict(zones)
        missing = [w for w in wanted if w not in by_type]
        if missing:
            sys.exit("No thermal zone of type %s; present: %s"
                     % (", ".join(missing), ", ".join(t for t, _ in zones)))
        zones = [(w, by_type[w]) for w in wanted]
    else:
        zones = [(t, p) for t, p in zones if not t.startswith("INT3400")]
        zones = ([z for c in CPU_ZONES for z in zones if z[0] == c]
                 + [z for z in zones if z[0] not in CPU_ZONES])
    if len(zones) > MAX_ZONES:
        print("%d zones; plotting the first %d (choose with --zones): %s"
              % (len(zones), MAX_ZONES, ", ".join(t for t, _ in zones[:MAX_ZONES])),
              file=sys.stderr)
        zones = zones[:MAX_ZONES]
    return zones


def sample(rapl, zones, t0):
    s = {"t": time.monotonic() - t0, "power": rapl.power_w(),
         "pl1": rapl.limit_w(0), "pl2": rapl.limit_w(1), "temps": {}}
    for name, path in zones:
        mc = read_int(path)
        if mc is not None:
            s["temps"][name] = mc / 1000
    return s


def fmt(v, unit):
    return "-" if v is None else "%.1f %s" % (v, unit)


class Plot:
    def __init__(self, zones, rapl, window, save):
        import matplotlib
        if save:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        self.plt = plt
        self.window = window
        self.save = save
        self.names = [n for n, _ in zones]
        plt.rcParams["lines.solid_capstyle"] = "round"
        plt.rcParams["lines.solid_joinstyle"] = "round"
        self.fig, (self.top, self.bottom) = plt.subplots(
            2, 1, sharex=True, figsize=(10, 6.5),
            gridspec_kw={"height_ratios": [1, 1], "hspace": 0.32})
        self.fig.patch.set_facecolor(SURFACE)
        try:
            self.fig.canvas.manager.set_window_title("thermald live plot")
        except AttributeError:
            pass
        for ax in (self.top, self.bottom):
            ax.set_facecolor(SURFACE)
            for side in ("top", "right"):
                ax.spines[side].set_visible(False)
            for side in ("left", "bottom"):
                ax.spines[side].set_color(BASELINE)
            ax.tick_params(colors=MUTED, labelcolor=TEXT_SECONDARY, labelsize=9)
            ax.grid(axis="y", color=GRID, linewidth=1)
            ax.set_axisbelow(True)

        self.temp_lines = {n: self.top.plot([], [], color=SERIES[i], linewidth=2,
                                            label=n)[0]
                           for i, n in enumerate(self.names)}
        self.power_line = self.bottom.plot([], [], color=SERIES[0], linewidth=2,
                                           label="package power")[0]
        self.pl1_line = self.bottom.plot([], [], color=SERIES[1], linewidth=2,
                                         drawstyle="steps-post", label="PL1")[0]
        self.pl2_line = self.bottom.plot([], [], color=SERIES[2], linewidth=2,
                                         drawstyle="steps-post", label="PL2")[0]
        self.end_marks = []

        self.top.set_ylabel("Temperature (°C)", color=TEXT_SECONDARY, fontsize=10)
        self.bottom.set_ylabel("Power (W)", color=TEXT_SECONDARY, fontsize=10)
        self.bottom.set_xlabel("Time (s)", color=TEXT_SECONDARY, fontsize=10)
        for ax in (self.top, self.bottom):
            leg = ax.legend(loc="lower left", bbox_to_anchor=(0, 1.0), frameon=False,
                            fontsize=9, ncol=4, handlelength=1.6, borderaxespad=0.2)
            for text in leg.get_texts():
                text.set_color(TEXT_PRIMARY)
        self.fig.suptitle("Package power, power limits and temperatures",
                          x=0.07, ha="left", fontsize=12, color=TEXT_PRIMARY)
        self.fig.text(0.07, 0.935, "RAPL %s · sampled every %s s"
                      % (rapl.name(), "%g" % args.interval),
                      fontsize=8.5, color=TEXT_SECONDARY)
        self.fig.subplots_adjust(left=0.07, right=0.8, top=0.84, bottom=0.08)
        if not save:
            plt.ion()
            plt.show(block=False)

    def place_end_labels(self, ax, items, span):
        """A dot at the end of each line, and a label with its name and
        value; lines ending close together share one label naming all of
        them. Text in text ink, the dot carries the color."""
        groups = []
        for x, y, name, value, color in sorted(items, key=lambda i: -i[1]):
            dot = ax.plot([x], [y], "o", markersize=8, color=color,
                          markeredgecolor=SURFACE, markeredgewidth=2, zorder=4)[0]
            self.end_marks.append(dot)
            if groups and abs(groups[-1]["y"] - y) < 0.06 * span:
                groups[-1]["names"].append(name)
            else:
                groups.append({"x": x, "y": y, "names": [name], "value": value})
        for g in groups:
            names = ", ".join(g["names"][:2])
            if len(g["names"]) > 2:         # the legend names the others
                names += " +%d" % (len(g["names"]) - 2)
            label = ax.annotate("%s %s" % (names, g["value"]),
                                (g["x"], g["y"]), xytext=(8, 0),
                                textcoords="offset points", va="center", fontsize=9,
                                color=TEXT_PRIMARY, annotation_clip=False)
            self.end_marks.append(label)

    def update(self, trace):
        for m in self.end_marks:
            m.remove()
        self.end_marks = []
        t_end = trace[-1]["t"]
        t_start = max(0.0, t_end - self.window)
        shown = [s for s in trace if s["t"] >= t_start]

        items, temps = [], []
        for i, n in enumerate(self.names):
            pts = [(s["t"], s["temps"][n]) for s in shown if n in s["temps"]]
            self.temp_lines[n].set_data([p[0] for p in pts], [p[1] for p in pts])
            if pts:
                temps += [p[1] for p in pts]
                items.append((pts[-1][0], pts[-1][1], n, "%.0f" % pts[-1][1],
                              SERIES[i]))
        if temps:
            lo, hi = min(temps), max(temps)
            pad = max(2.0, (hi - lo) * 0.1)
            self.top.set_ylim(lo - pad, hi + pad)
            self.place_end_labels(self.top, items, (hi - lo) + 2 * pad)

        items, watts = [], []
        for line, key, label, color in ((self.power_line, "power", "package", SERIES[0]),
                                        (self.pl1_line, "pl1", "PL1", SERIES[1]),
                                        (self.pl2_line, "pl2", "PL2", SERIES[2])):
            pts = [(s["t"], s[key]) for s in shown if s[key] is not None]
            line.set_data([p[0] for p in pts], [p[1] for p in pts])
            if pts:
                watts += [p[1] for p in pts]
                items.append((pts[-1][0], pts[-1][1], label, "%.1f" % pts[-1][1],
                              color))
        if watts:
            top = max(watts) * 1.12 or 1
            self.bottom.set_ylim(0, top)
            self.place_end_labels(self.bottom, items, top)

        self.bottom.set_xlim(t_start, max(t_start + 10, t_end) + 0.06 * self.window)
        if self.save:
            self.fig.savefig(self.save, facecolor=SURFACE, dpi=120)
        else:
            self.fig.canvas.draw_idle()
            self.fig.canvas.flush_events()

    def open(self):
        return self.save or self.plt.fignum_exists(self.fig.number)

    def pause(self, seconds):
        if self.save:
            time.sleep(seconds)
        else:
            self.plt.pause(seconds)


def main():
    global args
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between samples")
    ap.add_argument("--window", type=float, default=300, help="seconds shown")
    ap.add_argument("--zones", help="comma separated zone types (default: all but INT3400, "
                    "up to %d)" % MAX_ZONES)
    ap.add_argument("--domain", choices=("auto", "mmio", "msr"), default="auto",
                    help="RAPL domain for power and limits (default: MMIO if present)")
    ap.add_argument("--csv", help="also write every sample to this file")
    ap.add_argument("--save", help="no window: rewrite this PNG at every update")
    ap.add_argument("--no-plot", action="store_true", help="print only")
    ap.add_argument("--quiet", action="store_true", help="don't print samples")
    ap.add_argument("--duration", type=float, help="stop after this many seconds")
    args = ap.parse_args()
    if args.interval <= 0:
        ap.error("--interval must be positive")

    rapl = Rapl(args.domain)
    zones = find_zones(args.zones.split(",") if args.zones else None)
    names = [n for n, _ in zones]

    writer = None
    if args.csv:
        csv_file = open(args.csv, "w", newline="")
        writer = csv.writer(csv_file)
        writer.writerow(["t_s", "package_power_w", "pl1_w", "pl2_w"]
                        + ["%s_c" % n for n in names])

    plot = None
    if not args.no_plot:
        try:
            plot = Plot(zones, rapl, args.window, args.save)
        except ImportError:
            sys.exit("Needs python3-matplotlib (or use --no-plot)")
        except Exception as e:      # no display
            sys.exit("Can't open a window (%s): run with sudo -E from the desktop "
                     "session, or use --save FILE or --no-plot" % e)

    if not args.quiet:
        print("t(s)  power  PL1  PL2  " + "  ".join(names))
    t0 = time.monotonic()
    rapl.power_w()                  # first energy reading
    trace = []
    try:
        while True:
            if plot:
                plot.pause(args.interval)
                if not plot.open():
                    break
            else:
                time.sleep(args.interval)
            s = sample(rapl, zones, t0)
            trace.append(s)
            # keep a little more than the window
            while trace and trace[0]["t"] < s["t"] - args.window * 1.1:
                trace.pop(0)
            if not args.quiet:
                print("%5.0f  %s  %s  %s  %s" % (
                    s["t"], fmt(s["power"], "W"), fmt(s["pl1"], "W"), fmt(s["pl2"], "W"),
                    "  ".join(fmt(s["temps"].get(n), "C") for n in names)), flush=True)
            if writer:
                writer.writerow(["%.1f" % s["t"]] + [
                    "" if s[k] is None else "%.3f" % s[k] for k in ("power", "pl1", "pl2")]
                    + ["" if s["temps"].get(n) is None else "%.1f" % s["temps"][n]
                       for n in names])
                csv_file.flush()
            if plot:
                plot.update(trace)
            if args.duration and s["t"] >= args.duration:
                break
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
