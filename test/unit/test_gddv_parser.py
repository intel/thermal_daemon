"""Unit tests of thermald's GDDV parser, through build/gddv_dump.

Vaults are built with test/regression/lib/gddv.py, parsed by thermald's
own parser (compiled with ASan and UBSan) and compared field by field.
Malformed vaults must be rejected or parsed without a sanitizer error.

    make -C test/unit && pytest test/unit
    THD_VAULT_DIR=/private/vaults pytest test/unit   # also real vaults
"""

import glob
import json
import os
import random
import subprocess
import sys
import tempfile

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "regression", "lib"))
import gddv  # noqa: E402

DUMP = os.path.join(HERE, "build", "gddv_dump")
ENV = dict(os.environ,
           ASAN_OPTIONS="detect_leaks=1:abort_on_error=0:exitcode=99",
           UBSAN_OPTIONS="print_stacktrace=1:exitcode=99")


def parse(vault):
    """Run the parser, return (exit status, parsed JSON or None)."""
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        f.write(vault)
        f.flush()
        p = subprocess.run([DUMP, f.name], capture_output=True, env=ENV,
                           timeout=30)
    if p.returncode not in (0, 1):
        pytest.fail("gddv_dump exit %d (sanitizer error or crash):\n%s"
                    % (p.returncode, p.stderr.decode(errors="replace")[-3000:]),
                    pytrace=False)
    return p.returncode, json.loads(p.stdout)


def parsed(vault):
    status, out = parse(vault)
    assert status == 0 and out["status"] == "ok", out
    return out


@pytest.fixture(scope="session", autouse=True)
def built():
    if not os.path.exists(DUMP):
        pytest.exit("build first: make -C test/unit", returncode=2)


# Round trips

def test_psvt_entries():
    entries = [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50, limit="MIN"),
               gddv.psv(gddv.TCPU, "\\_SB_.SEN3", 60, limit=20000,
                        sample_period=20, priority=3),
               gddv.psv("\\_SB_.PC00.TFN1", "\\_SB_.SEN4", 70, limit="MAX")]
    out = parsed(gddv.encode({"psvt": {"IETM.D0": entries,
                                       "other": entries[:1]}}))
    assert set(out["psvt"]) == {"IETM.D0", "other"}
    got = out["psvt"]["IETM.D0"]
    assert [p["target"] for p in got] == [e["target"] for e in entries]
    assert [p["limit"] for p in got] == ["MIN", "20000", "MAX"]
    assert [p["temp"] for p in got] == [gddv.celsius_to_dk(e["temperature_c"])
                                        for e in entries]
    assert got[1]["sample_period"] == 20 and got[1]["priority"] == 3


def test_ppcc_two_limits():
    out = parsed(gddv.encode({"ppcc": {"TCPU.D0": gddv.ppcc(
        10000, 35000, 28000, 32000, 500, 35000, 64000, 2, 2, 1000)}}))
    assert out["ppcc"] == [{
        "name": "TCPU.D0", "pl1_min": 10000, "pl1_max": 35000,
        "pl1_twin_min": 28000, "pl1_twin_max": 32000, "pl1_step": 500,
        "pl2_valid": 1, "pl2_min": 35000, "pl2_max": 64000}]


def test_ppcc_pl1_only():
    """A PPCC without a PL2 entry (84 bytes) is kept, PL2 marked invalid.
    (The PSVT only makes the vault larger than struct header, which
    parse_gddv() requires.)"""
    out = parsed(gddv.encode({
        "ppcc_nr_limits": 1,
        "ppcc": {"TCPU.D0": gddv.ppcc(10000, 35000, 28000, 32000, 500)},
        "psvt": {"IETM.D0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)]}}))
    assert out["ppcc"] == [{
        "name": "TCPU.D0", "pl1_min": 10000, "pl1_max": 35000,
        "pl1_twin_min": 28000, "pl1_twin_max": 32000, "pl1_step": 500,
        "pl2_valid": 0, "pl2_min": 0, "pl2_max": 0}]


def test_apct_apat():
    apct = [(3, [gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
            (2, [gddv.cond(gddv.OEM0, gddv.EQUAL, 5),
                 gddv.temp_cond("\\_SB_.SEN2", gddv.GREATER_OR_EQUAL, 50),
                 gddv.cond(gddv.POWER_SLIDER, gddv.NOT_EQUAL, 100)]),
            (1, [gddv.cond(gddv.DEFAULT)])]
    apat = [gddv.action(1, "AC", gddv.TCPU, "PL1MAX", 30000),
            gddv.action(1, "AC", gddv.IETM, "PSVT", "IETM.D0"),
            gddv.action(2, "Hot", gddv.IETM, "NotifyPlatform", 5)]
    out = parsed(gddv.encode({"apct": apct, "apat": apat}))

    assert [[c["target"] for c in s] for s in out["apct"]] == [[3], [2, 2, 2], [1]]
    hot = out["apct"][1]
    assert [c["condition"] for c in hot] == [gddv.OEM0, gddv.TEMPERATURE,
                                             gddv.POWER_SLIDER]
    assert hot[1]["device"] == "\\_SB_.SEN2"
    assert hot[1]["argument"] == 50 * 10 + 2732
    assert hot[2]["comparison"] == gddv.NOT_EQUAL

    assert [(t["target_id"], t["code"], t["argument"]) for t in out["apat"]] \
        == [(1, "PL1MAX", "30000"), (1, "PSVT", "IETM.D0"),
            (2, "NotifyPlatform", "5")]


def test_two_virtual_sensors_stay_separate():
    """6656673: VSCT/VSPT of VR1.D0 and VR2.D0 must not be merged."""
    out = parsed(gddv.encode({
        "vsct": {"VR1.D0": [("\\_SB_.SEN3", 1000, 1000)],
                 "VR2.D0": [("\\_SB_.SEN4", 500, 250),
                            ("\\_SB_.SEN5", 500, 250)]},
        "vspt": {"VR1.D0": [(0, 1)], "VR2.D0": [(0, 2), (60, 1)]}}))
    vs = out["vsensors"]
    assert set(vs) == {"VR1.D0", "VR2.D0"}
    assert [t["target"] for t in vs["VR1.D0"]["vsct"]] == ["\\_SB_.SEN3"]
    assert [t["target"] for t in vs["VR2.D0"]["vsct"]] == ["\\_SB_.SEN4",
                                                          "\\_SB_.SEN5"]
    assert [p["period"] for p in vs["VR2.D0"]["vspt"]] == [2, 1]


def test_itmt_v1_short_last_entry():
    """d0929bb: a v1 ITMT ending right after pl1_max still parses."""
    vault = gddv.encode({
        "ppcc": {"TCPU.D0": gddv.ppcc(10000, 35000, 28000, 32000, 500,
                                       35000, 64000, 2, 2, 1000)},
        "raw": {"/shared/export/itmt": gddv.itmt_v1(
            [("\\_SB_.PC00.LPCB.EC0_.SEN1", 62, 28000, 35000)],
            short_last=True)}})
    out = parsed(vault)
    assert out["ppcc"][0]["pl1_max"] == 35000
    entries = out["itmt"][0]["entries"]
    assert [(e["pl1_min"], e["pl1_max"]) for e in entries] == [("28000", "35000")]


def test_itmt_v1_two_full_entries():
    out = parsed(gddv.encode({"raw": {"/shared/export/itmt": gddv.itmt_v1(
        [("\\_SB_.SEN1", 60, 15000, 25000), ("\\_SB_.SEN1", 70, 10000, 15000)])}}))
    assert len(out["itmt"][0]["entries"]) == 2


# Malformed vaults

def valid_vault():
    return gddv.encode({
        "ppcc": {"TCPU.D0": gddv.ppcc(10000, 35000, 28000, 32000, 500,
                                       35000, 64000, 2, 2, 1000)},
        "psvt": {"IETM.D0": [gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)]},
        "apct": [(2, [gddv.cond(gddv.OEM0, gddv.EQUAL, 5),
                      gddv.cond(gddv.LID_STATE, gddv.EQUAL, 0)]),
                 (1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "AC", gddv.TCPU, "PL1MAX", 30000)],
        "vsct": {"VR1.D0": [("\\_SB_.SEN3", 1000, 1000)]},
        "vspt": {"VR1.D0": [(0, 1)]},
        "raw": {"/shared/export/itmt": gddv.itmt_v1(
            [("\\_SB_.SEN1", 62, 28000, 35000)], short_last=True)},
    })


def test_every_truncation():
    """Every prefix of a valid vault: no crash or sanitizer error."""
    vault = valid_vault()
    for size in range(len(vault)):
        parse(vault[:size])


@pytest.mark.parametrize("seed", range(4))
def test_random_mutations(seed):
    """Deterministic byte mutations of a valid vault."""
    vault = bytearray(valid_vault())
    rng = random.Random(seed)
    for _ in range(100):
        mutated = bytearray(vault)
        for _ in range(rng.randint(1, 8)):
            pos = rng.randrange(len(mutated))
            mutated[pos] = rng.choice([0x00, 0xff, 0x7f, 0x80,
                                       rng.randrange(256)])
        parse(bytes(mutated))


@pytest.mark.parametrize("length", [0x7ffffff0, 0xffffffff, 0x80000000])
def test_huge_lengths(length):
    import struct
    parse(gddv.HEADER + struct.pack("<II", 0, length) + b"/x\0")
    parse(gddv.HEADER + gddv.key("/shared/tables/psvt/x",
                                 gddv.u64(2) + struct.pack("<IQ", gddv.TYPE_STRING,
                                                           length)))


@pytest.mark.parametrize("where", ["key", "value", "string"])
def test_length_near_int_max(where):
    """Found by test/fuzz: the length checks computed offset + length,
    which overflowed int for lengths near INT_MAX, so they passed and
    the copy that followed read past the buffer."""
    import struct
    near_max = 0x7ffffffd
    # parse_gddv() ignores buffers smaller than struct header (which
    # includes the larger v2 header) and trailing bytes shorter than the
    # header, so pad the vault to make sure the bad item is reached
    pad = b"\0" * 512
    if where == "key":
        vault = gddv.HEADER + struct.pack("<II", 0, near_max) + b"/x\0" + pad
    elif where == "value":
        vault = gddv.HEADER + struct.pack("<II", 0, 3) + b"/x\0" \
            + struct.pack("<II", 0, near_max) + b"abc" + pad
    else:
        vault = gddv.HEADER + gddv.key(
            "/shared/tables/psvt/x", gddv.u64(2)
            + struct.pack("<IQ", gddv.TYPE_STRING, near_max) + b"abc") + pad
    parse(vault)


def lzma_alone(data):
    """LZMA "alone" stream with the uncompressed size filled in, as in
    firmware vaults (Python writes "unknown", which thermald rejects)."""
    import lzma
    import struct
    raw = bytearray(lzma.compress(data, format=lzma.FORMAT_ALONE))
    raw[5:13] = struct.pack("<Q", len(data))
    return bytes(raw)


def test_compressed_vault_with_short_header():
    """Found by test/fuzz: a compressed v2 vault whose headersize is
    smaller than the v2 header made handle_compressed_gddv() write the
    v2 fields past the end of the decompressed buffer."""
    import lzma
    import struct
    # A tiny payload: the decompressed buffer (headersize + 12 bytes)
    # ends long before the v2 fields at offsets up to 148
    payload = lzma_alone(gddv.HEADER)
    for headersize in (2, 12, 64, 147):
        header = struct.pack("<HHI", 0x1fe5, headersize, 2 << 24)
        header += struct.pack("<I", 0x40000000)        # compressed
        header = (header + b"\0" * 148)[:max(headersize, 12)]
        parse(header + payload + b"\0" * 64)


def test_compressed_vault():
    """A compressed v2 vault (as real firmware ships) still parses."""
    import lzma
    import struct
    inner = gddv.encode({"psvt": {"IETM.D0": [
        gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)]}})
    header = struct.pack("<HHI", 0x1fe5, 148, 2 << 24)
    header += struct.pack("<I", 0x40000000) + b"\0" * (148 - 12)
    out = parsed(header + lzma_alone(inner))
    assert "IETM.D0" in out["psvt"]


# Real vaults from a private directory (never stored in the repo)

VAULT_DIR = os.environ.get("THD_VAULT_DIR")
VAULTS = sorted(glob.glob(os.path.join(VAULT_DIR, "*.bin"))) if VAULT_DIR else []


@pytest.mark.skipif(not VAULTS, reason="set THD_VAULT_DIR to a directory "
                    "of data_vault dumps (*.bin)")
@pytest.mark.parametrize("path", VAULTS, ids=[os.path.basename(v) for v in VAULTS])
def test_real_vault(path):
    """A real vault must parse. With <name>.json next to it, its
    "parser" entry lists expected values, e.g.
    {"parser": {"psvt": ["IETM.D0"], "ppcc_pl1_max": 35000, "apat_targets": 9}}"""
    with open(path, "rb") as f:
        out = parsed(f.read())
    meta = os.path.splitext(path)[0] + ".json"
    if not os.path.exists(meta):
        return
    with open(meta) as f:
        expect = json.load(f).get("parser", {})
    if "psvt" in expect:
        assert set(expect["psvt"]) <= set(out["psvt"])
    if "ppcc_pl1_max" in expect:
        assert out["ppcc"] and out["ppcc"][0]["pl1_max"] == expect["ppcc_pl1_max"]
    if "apat_targets" in expect:
        assert len({t["target_id"] for t in out["apat"]}) == expect["apat_targets"]
    if "apct_sets" in expect:
        assert len(out["apct"]) == expect["apct_sets"]
    if "vsensors" in expect:
        assert set(expect["vsensors"]) <= set(out["vsensors"])
