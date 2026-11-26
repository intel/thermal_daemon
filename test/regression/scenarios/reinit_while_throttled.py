"""D-Bus Reinit while a PSVT is throttling PL1.

After Reinit thermald must keep controlling: when the zone cools PL1
returns to the PPCC maximum. On exit the original power-on PL1 is
restored, not a value thermald wrote before the Reinit.

thd_dbus_interface_reinit() used to create the default engine even when
thermald ran with --adaptive, so adaptive control was lost and PL1
stayed at the last throttled value.
"""

import gddv
from platforms import POWER_ON_PL1_W, ppcc, rapl

FEATURES = open(__file__.rsplit("/test/", 1)[0]
                + "/data/thermald-features.xml").read()

PLATFORM = {
    "dbus": True,
    "thermald_args": ["--adaptive", "--dbus-enable"],
    # The shipped default has D-Bus control disabled
    "conf_files": {"thermald-features.xml": FEATURES.replace(
        "<DbusControl> 0 </DbusControl>", "<DbusControl> 1 </DbusControl>")},
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": {
        "ppcc": {"TCPU.D0": ppcc(10, 35)},
        "psvt": {"IETM.D0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)]},
    },
}


def run(g):
    g.wait_ready()
    g.set_temp("SEN2", 60)
    g.wait_until(lambda: g.pl1("mmio") <= 15, 60,
                 lambda: "throttled before Reinit (PL1 %.1f W)" % g.pl1("mmio"))

    g.reinit()
    g.wait_log("Dbus method called org.freedesktop.thermald Reinit")
    g.wait_until(lambda: g.log().count("Start main loop") >= 1
                 and g.log().rfind("RAPL sysfs present")
                 > g.log().rfind("Reinit."), 30, "engine restarted")

    g.set_temp("SEN2", 40)
    g.wait_until(lambda: g.pl1("mmio") == 35, 60,
                 lambda: "after Reinit, cooled: PL1 back at 35 W (is %.1f W)"
                 % g.pl1("mmio"))
    g.check(g.thermald_running(), "thermald exited after Reinit")

    status = g.stop_thermald()
    g.check(status == 0, "exit status %s" % status)
    g.check(g.pl1("mmio") == POWER_ON_PL1_W,
            "MMIO PL1 after exit %.1f W, expected the original power-on %d W"
            % (g.pl1("mmio"), POWER_ON_PL1_W))
