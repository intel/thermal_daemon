"""A falling temperature may release PL1 above the set point."""

from platforms import itmt_pid_platform

PLATFORM = itmt_pid_platform("<AllowRaiseAboveSetPoint>1</AllowRaiseAboveSetPoint>")


def run(g):
    g.wait_ready()
    g.set_temp("SEN2", 55.05)
    g.wait_until(lambda: g.pl1("mmio") == 32, 25, "default backstop")
    g.set_temp("SEN2", 51.05)
    g.wait_until(lambda: g.pl1("mmio") > 33, 10, "release above 51 C")
    g.wait_log("allow raise 1")
