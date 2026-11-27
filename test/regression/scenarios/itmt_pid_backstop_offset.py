"""A 2 C backstop leaves the PID in control at trip + 1 C."""

from platforms import itmt_pid_platform

PLATFORM = itmt_pid_platform("<BackstopOffset>2000</BackstopOffset>")


def run(g):
    g.wait_ready()
    g.set_temp("SEN2", 52.5)
    g.wait_until(lambda: g.pl1("mmio") < 41, 15, "first PID cut")
    g.hold(lambda: g.pl1("mmio") > 38, 20, "backstop started below 2 C")
    g.set_temp("SEN2", 53.5)
    g.wait_until(lambda: g.pl1("mmio") == 32, 20, "backstop at exactly 2 C")
    g.wait_log("backstop offset 2000")
