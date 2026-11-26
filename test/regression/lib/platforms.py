"""Building blocks shared by scenarios."""

import gddv

POWER_ON_PL1_W = 15


# itmt-pid.xml enabling the ITMT PID with the default gains
ITMT_PID_ON = """<?xml version="1.0"?>
<ItmtPid>
	<Enable>1</Enable>
</ItmtPid>
"""


def ppcc(min_w=10, max_w=35, step_w=1):
    """PL1 min_w..max_w W, PL2 35-64 W."""
    return gddv.ppcc(pl1_min_mw=min_w * 1000, pl1_max_mw=max_w * 1000,
                     pl1_twin_min_ms=28000, pl1_twin_max_ms=32000,
                     pl1_step_mw=step_w * 1000,
                     pl2_min_mw=35000, pl2_max_mw=64000, pl2_twin_min_ms=2,
                     pl2_twin_max_ms=2, pl2_step_mw=1000)


def rapl(pl1_w=POWER_ON_PL1_W, demand_w=45, **extra):
    r = {"pl1_uw": pl1_w * 1000000, "max_uw": 64000000,
         "demand_uw": demand_w * 1000000}
    r.update(extra)
    return r


def pl1max_target(target_id, name, pl1max_w, psvt="IETM.D0"):
    """APAT rows for a target that installs a PSVT and caps PL1."""
    rows = []
    if psvt:
        rows.append(gddv.action(target_id, name, gddv.IETM, "PSVT", psvt))
    rows.append(gddv.action(target_id, name, gddv.TCPU, "PL1MAX",
                            pl1max_w * 1000))
    return rows


def idle_psvt(sensor="SEN2", trip_c=90):
    """A PSVT that never trips at test temperatures, so the targets'
    PL1MAX is the only thing setting PL1."""
    return [gddv.psv(gddv.TCPU, "\\_SB_." + sensor, trip_c, limit="MIN")]


def base_vault(**extra):
    """A small valid adaptive vault: PPCC 10-35 W and an idle IETM.D0."""
    vault = {"ppcc": {"TCPU.D0": ppcc()}, "psvt": {"IETM.D0": idle_psvt()}}
    vault.update(extra)
    return vault


def check_rejected_cleanly(g, seconds=10):
    """A bad vault: thermald must not crash or hang, and must exit
    cleanly. Whether it falls back to non-adaptive mode is up to it."""
    g.hold(lambda: g.thermald_running(), seconds,
           lambda: "thermald died (status %s)" % g.proc.returncode)
    for bad in ("Segmentation", "AddressSanitizer", "terminate called",
                "Aborted"):
        g.check(bad not in g.log(), "thermald log contains %r" % bad)
    status = g.stop_thermald()
    g.check(status == 0, "thermald exit status %s" % status)
