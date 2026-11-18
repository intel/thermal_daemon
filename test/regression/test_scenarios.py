"""Run every scenarios/*.py in its own VM.

    pytest test/regression                  # everything
    pytest test/regression -k restore       # scenarios matching "restore"

Results, thermald logs and traces go to $THD_REG_OUT/<scenario>/
(default /tmp/thermald-regression).

A scenario whose PLATFORM has "known_bug": "<description>" checks for a
thermald bug that isn't fixed yet. It is reported as xfail while it
fails, and as a failure once it passes, so the marker gets removed with
the fix.
"""

import glob
import importlib.util
import os
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lib"))
import vm  # noqa: E402

import real_vault  # noqa: E402

OUT = os.environ.get("THD_REG_OUT", "/tmp/thermald-regression")
SCENARIOS = sorted(glob.glob(os.path.join(HERE, "scenarios", "*.py")))

# Real data vaults from a private directory: run.sh --vault-dir DIR or
# THD_VAULT_DIR. They may hold OEM data and are never copied into the repo.
VAULT_DIR = os.environ.get("THD_VAULT_DIR")
VAULTS = real_vault.vaults(VAULT_DIR) if VAULT_DIR else []
REAL_SCENARIO = os.path.join(HERE, "real", "real_vault.py")


def load_platform(path):
    spec = importlib.util.spec_from_file_location("scenario", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.PLATFORM


@pytest.fixture(scope="session", autouse=True)
def parser_tool():
    """lib/real_vault.py needs test/unit/build/gddv_dump."""
    if VAULTS and not os.path.exists(real_vault.DUMP):
        pytest.exit("build test/unit first (run.sh does)", returncode=2)


@pytest.mark.parametrize("scenario", SCENARIOS,
                         ids=[os.path.basename(s)[:-3] for s in SCENARIOS])
def test_scenario(scenario):
    name = os.path.basename(scenario)[:-3]
    outdir = os.path.join(OUT, name)
    platform = load_platform(scenario)
    result = vm.run_scenario(scenario, outdir, platform)
    known_bug = platform.get("known_bug")

    if known_bug:
        if result.get("passed"):
            pytest.fail("Known bug no longer reproduces, remove "
                        "\"known_bug\" from the scenario: %s" % known_bug,
                        pytrace=False)
        pytest.xfail("%s (%s)" % (known_bug, result.get("error", "failed")))

    if not result.get("passed"):
        log = os.path.join(outdir, "thermald.log")
        tail = ""
        if os.path.exists(log):
            with open(log, errors="replace") as f:
                tail = "".join(f.readlines()[-40:])
        pytest.fail("%s\n\nLast thermald log lines (%s):\n%s"
                    % (result.get("error", "failed"), log, tail),
                    pytrace=False)


@pytest.mark.skipif(not VAULTS, reason="no real vaults: run.sh --vault-dir DIR")
@pytest.mark.parametrize("vault", VAULTS,
                         ids=[os.path.basename(v)[:-4] for v in VAULTS])
def test_real_vault(vault):
    name = "real_" + os.path.basename(vault)[:-4]
    outdir = os.path.join(OUT, name)
    result = vm.run_scenario(REAL_SCENARIO, outdir,
                             real_vault.platform_for(vault))
    if not result.get("passed"):
        log = os.path.join(outdir, "thermald.log")
        tail = ""
        if os.path.exists(log):
            with open(log, errors="replace") as f:
                tail = "".join(f.readlines()[-40:])
        pytest.fail("%s: %s\n\nLast thermald log lines (%s):\n%s"
                    % (os.path.basename(vault), result.get("error", "failed"),
                       log, tail), pytrace=False)
