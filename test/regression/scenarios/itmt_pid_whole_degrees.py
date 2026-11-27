"""A notebook with a skin sensor that reports whole degrees."""

from platforms import itmt_pid_platform

PLATFORM = itmt_pid_platform("<BackstopOffset>0</BackstopOffset>"
                             "<AllowRaiseAboveSetPoint>1</AllowRaiseAboveSetPoint>")


def run(g):
    g.wait_ready()
    start = len(g.rapl_writes("mmio"))
    g.set_temp("SEN2", 52.55)
    g.wait_until(lambda: g.pl1("mmio") < 41, 15, "first PID cut")
    g.hold(lambda: g.pl1("mmio") > 32, 60, "fully limited at trip + 1 C")
    plateau = g.pl1("mmio")
    g.set_temp("SEN2", 53.55)
    g.wait_until(lambda: g.pl1("mmio") < plateau - 0.5, 10, "first rise")
    warm = g.pl1("mmio")
    g.set_temp("SEN2", 54.55)
    g.wait_until(lambda: g.pl1("mmio") < warm - 0.5, 10, "second rise")
    peak = g.pl1("mmio")
    g.set_temp("SEN2", 52.55)
    g.wait_until(lambda: g.pl1("mmio") > peak + 0.5, 10, "release above trip")
    writes = g.rapl_writes("mmio")[start:]
    g.check(all(32000000 < w["value"] <= 41000000 for w in writes),
            "PL1 reached the bottom of the range: %s" % writes)
