"""Build a test platform around a real data vault.

The vault is parsed with test/unit/build/gddv_dump (thermald's own
parser). Every sensor its tables refer to becomes a test thermal zone,
every PSVT source other than the CPU becomes a test cooling device, and
fake RAPL is set up for TCPU. An optional <vault>.json next to the vault
adds what the vault can't tell:

    {
      "odvp": [0, 1, 0],          OEM variables (ACPI ODVP)
      "cpu_model": 204,           family 6 model, default Meteor Lake L
      "upower": {"on_battery": false},
      "power_profile": "balanced",
      "switches": ["lid"],
      "vm": {
        "target": "U_CS_B_A",     adaptive target expected at start
        "pl1_w": 28               MMIO PL1 expected at start
      }
    }

Vaults stay where they are (THD_VAULT_DIR); nothing is copied into the
repo.
"""

import glob
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
DUMP = os.path.join(HERE, "..", "..", "unit", "build", "gddv_dump")
CPU_SOURCES = ("TCPU", "B0D4")


def participant(path):
    """\\_SB_.PC00.LPCB.EC0_.SEN1 -> SEN1, as thermald names zones."""
    return path.rsplit(".", 1)[-1].rstrip("_")


def vaults(directory):
    return sorted(glob.glob(os.path.join(directory, "*.bin")))


def parse(vault_path):
    p = subprocess.run([DUMP, vault_path], capture_output=True, text=True,
                       timeout=60)
    if p.returncode not in (0, 1):
        raise RuntimeError("gddv_dump failed on %s:\n%s"
                           % (vault_path, p.stderr[-2000:]))
    return json.loads(p.stdout)


def platform_for(vault_path):
    tables = parse(vault_path)
    virtual = {name.split(".")[0] for name in tables.get("vsensors", {})}
    sensors, cdevs = set(), set()

    for entries in tables.get("psvt", {}).values():
        for e in entries:
            sensors.add(participant(e["target"]))
            source = participant(e["source"])
            if source not in CPU_SOURCES:
                cdevs.add(source)
    for vs in tables.get("vsensors", {}).values():
        for t in vs["vsct"]:
            sensors.add(participant(t["target"]))
    for conditions in tables.get("apct", []):
        for c in conditions:
            if c["device"]:
                sensors.add(participant(c["device"]))
    for itmt in tables.get("itmt", []):
        for e in itmt["entries"]:
            sensors.add(participant(e["target"]))
    sensors -= virtual | {"IETM", ""}

    with open(vault_path, "rb") as f:
        raw = f.read()
    platform = {
        "real_vault": os.path.basename(vault_path),
        "parse_status": tables["status"],
        "ppcc": tables.get("ppcc", []),
        "zones": sorted(sensors),
        "initial_temps": {s: 35 for s in sensors},
        "cdevs": sorted(cdevs | {"intel_powerclamp"}),
        "rapl": {"pl1_uw": 15000000, "max_uw": 64000000,
                 "demand_uw": 45000000},
        "gddv": {"raw_vault": raw},
    }

    meta = os.path.splitext(vault_path)[0] + ".json"
    if os.path.exists(meta):
        with open(meta) as f:
            extra = json.load(f)
        for k in ("odvp", "cpu_model", "upower", "power_profile",
                  "switches", "vm"):
            if k in extra:
                platform[k] = extra[k]
    return platform
