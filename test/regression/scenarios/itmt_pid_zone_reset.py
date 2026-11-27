"""A gradual PID starts fresh after its zone stops polling."""

from platforms import itmt_pid_platform

PLATFORM = itmt_pid_platform("")


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == 41, 30, "initial PL1")
    g.set_temp("SEN2", 51.55)
    g.wait_until(lambda: g.pl1("mmio") < 41, 15, "first cut")
    g.set_temp("SEN2", 55.05)
    g.wait_until(lambda: g.pl1("mmio") == 32, 30, "floor")
    g.set_temp("SEN2", 45.05)
    g.wait_until(lambda: g.pl1("mmio") == 41, 30, "zone reset")
    start = len(g.log())
    g.set_temp("SEN2", 51.55)
    g.wait_until(lambda: g.pl1("mmio") < 41, 15, "cut after reset")
    g.check("pid first call mode:inc e:550" in g.log()[start:], "PID kept the previous sample")
