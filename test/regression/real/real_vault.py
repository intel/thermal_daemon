"""Generic checks for a real data vault (see lib/real_vault.py).

PLATFORM is built per vault on the host and passed to the guest.

  - thermald stays in adaptive mode: the vault parses, every PSVT
    target has a zone, no fallback to the default engine
  - PL1 stays within the vault's PPCC range while running
  - optional expectations from <vault>.json: initial target and PL1
  - SIGTERM: clean exit, power-on PL1 restored
"""

PLATFORM = {}       # replaced by the host for each vault


def run(g):
    p = g.platform
    g.check(p["parse_status"] == "ok", "thermald's parser rejects the vault")
    g.wait_ready(timeout=40)
    g.hold(lambda: g.thermald_running(), 10,
           lambda: "thermald exited (status %s)" % g.proc.returncode)

    log = g.log()
    for bad, what in (("--adaptive option failed", "fell back to non-adaptive"),
                      ("Unable to find a zone", "a PSVT target has no zone"),
                      ("couldn't create any zones", "no adaptive zones"),
                      ("GDDV parsing failed", "vault rejected")):
        g.check(bad not in log, what)

    ppcc = [x for x in p.get("ppcc", []) if x["name"] == "TCPU.D0"]
    if ppcc:
        lo, hi = ppcc[0]["pl1_min"] / 1000, ppcc[0]["pl1_max"] / 1000
        for w in g.rapl_writes("mmio"):
            g.check(lo <= w["value"] / 1e6 <= hi or w["ret"],
                    "MMIO PL1 %.1f W written, PPCC range %g-%g W"
                    % (w["value"] / 1e6, lo, hi))

    expect = p.get("vm", {})
    if "target" in expect:
        g.check("Target Name:%s" % expect["target"] in log,
                "initial target %s not selected" % expect["target"])
    if "pl1_w" in expect:
        g.wait_until(lambda: g.pl1("mmio") == expect["pl1_w"], 20,
                     lambda: "MMIO PL1 %s W (is %.1f W)"
                     % (expect["pl1_w"], g.pl1("mmio")))

    status = g.stop_thermald()
    g.check(status == 0, "thermald exit status %s" % status)
    g.check(g.pl1("mmio") == 15, "power-on PL1 not restored (%.1f W)"
            % g.pl1("mmio"))
