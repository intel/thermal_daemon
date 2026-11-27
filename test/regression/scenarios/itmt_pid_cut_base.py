"""Cuts near the set point start from package power or the last requested PL1."""

from pathlib import Path
import time

from guest import CONF_DIR
from platforms import itmt_pid_platform

SETTINGS = ("<Kp>25</Kp><Kd>0</Kd><ScaleWithRange>0</ScaleWithRange>"
            "<SetPointOffset>0</SetPointOffset>")
PLATFORM = itmt_pid_platform(SETTINGS)


def check_cuts(g, from_limit):
    g.wait_until(lambda: g.pl1("mmio") == 41, 30, "initial PL1")
    g.set_demand(40.7)
    time.sleep(3)
    before = g.pl1("mmio")
    g.set_temp("SEN2", 51.55)
    for sample in range(4):
        g.wait_until(lambda: g.pl1("mmio") < before, 15, "next PID cut")
        limit = g.pl1("mmio")
        cut = before - limit
        if from_limit:
            # At +50 mC the first cut is Kp * 50, then Kp * 50 * elapsed seconds.
            seconds = round(cut * 1e6 / 1250)
            g.check(abs(cut * 1e6 - 1250 * seconds) < 1 and
                    (seconds == 1 if sample == 0 else 1 <= seconds <= 10),
                    "cut from %.6f to %.6f W" % (before, limit))
        else:
            g.check(0.2 < cut < 0.45, "cut %.6f W from measured power" % cut)
        before = limit
        g.set_demand(limit - 0.3)


def run(g):
    g.wait_ready()
    check_cuts(g, False)
    g.check(g.stop_thermald() == 0, "stop before changing cut base")
    g.set_temp("SEN2", 45)
    path = Path(CONF_DIR) / "itmt-pid.xml"
    config = itmt_pid_platform(SETTINGS + "<CutFromLastLimit>1</CutFromLastLimit>")
    path.write_text(config["conf_files"]["itmt-pid.xml"])
    g.start_thermald()
    check_cuts(g, True)
    g.wait_log("cut from last limit 1")
