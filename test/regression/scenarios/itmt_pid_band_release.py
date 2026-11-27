"""A trip that fully releases PL1 in its band lets a peer take over."""

from platforms import itmt_pid_platform

PLATFORM = itmt_pid_platform("")
PLATFORM["zones"].append("SEN3")
PLATFORM["initial_temps"]["SEN3"] = 45
PLATFORM["gddv"]["itmt"]["IETM.D0"].append(("\\_SB_.SEN3", 52, 35000, "MAX"))


def run(g):
    g.wait_ready()
    g.wait_until(lambda: g.pl1("mmio") == 41, 30, "initial PL1")
    g.set_temp("SEN2", 51.55)
    g.wait_until(lambda: g.pl1("mmio") < 41, 15, "first cut")
    g.set_temp("SEN2", 50.05)
    g.wait_until(lambda: g.pl1("mmio") == 41, 30, "released inside band")
    g.set_temp("SEN2", 48.05)
    g.wait_log("Erased  [", 20)
    g.set_temp("SEN3", 56.05)
    g.wait_until(lambda: g.pl1("mmio") == 35, 30, "peer controls PL1")
