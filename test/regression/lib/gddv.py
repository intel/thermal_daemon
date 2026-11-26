"""Encode a test platform description into a GDDV data vault.

The format matches what thermald's cthd_gddv parser reads, and the PSVT
encoding is byte-identical to test/manual/data_vault/gddv_tests/gddv_test_gen.py.

    vault = encode({
        "psvt": {"IETM.D0": [psv(...), ...]},
        "ppcc": {"TCPU.D0": ppcc(...)},
        "itmt": {"IETM.D0": [(TCPU, 60, 25000, "MAX"), ...]},
        "idsp": [ITMT_UUID],
    })
"""

import struct
import uuid

TYPE_UINT64 = 4
TYPE_STRING = 8

# 12 byte header: signature 0x1FE5, header revision, version
HEADER = b"\xE5\x1F\x0C\x00\x00\x00\x00\x01\x00\x00\x00\x00"


def u64(value):
    return struct.pack("<IQ", TYPE_UINT64, int(value))


def string(value):
    raw = value.encode() + b"\x00"
    return struct.pack("<IQ", TYPE_STRING, len(raw)) + raw


def key(name, value):
    """One key/value item: flags, key length, key, value type, value length."""
    raw = name.encode() + b"\x00"
    return (struct.pack("<II", 0, len(raw)) + raw
            + struct.pack("<II", 0, len(value)) + value)


def celsius_to_dk(celsius):
    return int(celsius) * 10 + 2727


def psv(source, target, temperature_c, limit="MIN", priority=10,
        sample_period=10, domain=9, control_knob=65536, step_size=250,
        limit_coeff=10, unlimit_coeff=10, temperature_dk=None):
    """One PSVT entry. limit is "MIN", "MAX" or a power in mW.
    temperature_dk sets the raw deci-Kelvin value instead."""
    return dict(source=source, target=target, temperature_c=temperature_c,
                temperature_dk=temperature_dk,
                limit=limit, priority=priority, sample_period=sample_period,
                domain=domain, control_knob=control_knob,
                step_size=step_size, limit_coeff=limit_coeff,
                unlimit_coeff=unlimit_coeff)


def encode_psvt(entries, version=2):
    data = u64(version)
    for e in entries:
        data += string(e["source"]) + string(e["target"])
        data += u64(e["priority"]) + u64(e["sample_period"])
        dk = e.get("temperature_dk")
        data += u64(dk if dk is not None else celsius_to_dk(e["temperature_c"]))
        data += u64(e["domain"]) + u64(e["control_knob"])
        if str(e["limit"]).upper() in ("MIN", "MAX"):
            data += string(str(e["limit"]).upper())
        else:
            data += u64(e["limit"])
        data += u64(e["step_size"]) + u64(e["limit_coeff"])
        data += u64(e["unlimit_coeff"])
        data += b"\x00" * 12
    return data


def ppcc(pl1_min_mw, pl1_max_mw, pl1_twin_min_ms, pl1_twin_max_ms,
         pl1_step_mw, pl2_min_mw=0, pl2_max_mw=0, pl2_twin_min_ms=0,
         pl2_twin_max_ms=0, pl2_step_mw=0):
    """PPCC limits: power in mW, time windows in ms (as in ACPI PPCC)."""
    return [pl1_min_mw, pl1_max_mw, pl1_twin_min_ms, pl1_twin_max_ms,
            pl1_step_mw, pl2_min_mw, pl2_max_mw, pl2_twin_min_ms,
            pl2_twin_max_ms, pl2_step_mw]


def encode_ppcc(limits, revision=2, nr_limits=2):
    """Revision, then (index, min, max, twin min, twin max, step) per PL.

    nr_limits=1 encodes only PL1 (84 bytes), as some platforms do.
    """
    data = u64(revision)
    for index in range(nr_limits):
        data += u64(index)
        for value in limits[index * 5:index * 5 + 5]:
            data += u64(value)
    return data


# Condition IDs, comparisons and operations (enum adaptive_condition etc.
# in src/thd_gddv.h)
DEFAULT = 1
MOTION = 4                # thermald: always 0 (stationary)
POWER_SOURCE = 8          # 0: AC, 1: battery
LID_STATE = 10            # 0: closed, 1: open
PLATFORM_TYPE = 11        # 1: clamshell, 2: tablet
TEMPERATURE = 17          # argument in deci-Kelvin, device = sensor
OEM0 = 19                 # Oem0..Oem5: odvp0..odvp5
POWER_SLIDER = 56         # 25 power-saver, 75 balanced, 100 performance

EQUAL = 1
LESSER_OR_EQUAL = 2
GREATER_OR_EQUAL = 3
NOT_EQUAL = 4

AND = 1

IETM = "\\_SB_.IETM"
TCPU = "\\_SB_.PC00.TCPU"


def cond(condition, comparison=EQUAL, argument=0, device=""):
    """One APCT condition. Conditions of a set are ANDed."""
    return dict(condition=condition, comparison=comparison,
                argument=argument, device=device)


def temp_cond(sensor, comparison, celsius):
    """Temperature condition on a sensor. thermald compares
    millicelsius / 100 + 2732 with the argument (no hysteresis)."""
    return cond(TEMPERATURE, comparison, int(celsius) * 10 + 2732, sensor)


def encode_apct(condition_sets, version=2):
    """condition_sets: list of (target_id, [cond(), ...]), first match wins."""
    data = u64(version)
    for target, conditions in condition_sets:
        data += u64(target) + u64(len(conditions))
        for i, c in enumerate(conditions):
            data += u64(c["condition"]) + string(c["device"])
            data += u64(0)        # skipped by thermald
            data += u64(c["comparison"]) + u64(c["argument"])
            if i < len(conditions) - 1:
                data += u64(AND)
    return data


def action(target_id, name, participant, code, argument, domain=0):
    """One APAT entry: what target `target_id` does."""
    return dict(target_id=target_id, name=name, participant=participant,
                code=code, argument=str(argument), domain=domain)


def encode_apat(actions, version=2):
    data = u64(version)
    for a in actions:
        data += u64(a["target_id"]) + string(a["name"])
        data += string(a["participant"]) + u64(a["domain"])
        data += string(a["code"]) + string(a["argument"])
    return data


def encode_vsct(entries, version=1):
    """Virtual sensor calculation table: (target sensor, coeff, alpha) per
    entry, coeff and alpha in 1/1000 (1000 = 1.0, alpha 1000 = no
    smoothing). The virtual temperature is the sum of coeff * target."""
    data = u64(version)
    for target, coeff, alpha in entries:
        data += string(target) + u64(0) + u64(0)      # domain, coeff type
        data += u64(coeff) + u64(0) + u64(alpha)      # coeff, add, alpha
        data += u64(0)                                # trigger point
    return data


def encode_vspt(entries, version=1):
    """Virtual sensor polling table: (temperature C, period s) entries."""
    data = u64(version)
    for celsius, period_s in entries:
        data += u64(celsius_to_dk(celsius)) + u64(int(period_s * 10))
    return data


def itmt_v1(entries, short_last=False):
    """ITMT version 1: (target, trip C, pl1_min, pl1_max) per entry.

    short_last=True ends the last entry right after pl1_max, as on the
    ASUS Vivobook X1402ZA vault (see d0929bb).
    """
    data = u64(1)
    for i, (target, trip_c, pl1_min, pl1_max) in enumerate(entries):
        data += string(target) + u64(celsius_to_dk(trip_c))
        data += string(str(pl1_min)) + string(str(pl1_max))
        if short_last and i == len(entries) - 1:
            break
        data += string("") + b"\x00" * 12
    return data


# IDSP UUIDs thermald looks for before it uses an ITMT table
# (cthd_gddv::find_itmt)
ITMT_UUID = "6BD40D2D-98AA-A44B-1A92-D22BDE3117F1"         # ITMT v1, v2
ITMT3_UUID = "4215267F-F429-4776-9D84-D6C5992848A4"        # ITMT v3


def encode_idsp(uuids):
    """IDSP: per UUID, type 7, length 16, 4 bytes thermald skips, then the
    UUID in GUID byte order (as cthd_gddv::parse_idsp reads it)."""
    data = b""
    for u in uuids:
        data += struct.pack("<III", 7, 16, 0) + uuid.UUID(u).bytes_le
    return data


def encode(platform):
    if "raw_vault" in platform:
        return platform["raw_vault"]   # used as is, e.g. a corrupt header
    data = HEADER
    for name, entries in platform.get("psvt", {}).items():
        data += key("/shared/tables/psvt/" + name, encode_psvt(entries))
    for participant, limits in platform.get("ppcc", {}).items():
        data += key("/participants/" + participant + "/ppcc",
                    encode_ppcc(limits, nr_limits=platform.get(
                        "ppcc_nr_limits", 2)))
    for participant, entries in platform.get("vsct", {}).items():
        data += key("/participants/" + participant + "/vsct",
                    encode_vsct(entries))
    for participant, entries in platform.get("vspt", {}).items():
        data += key("/participants/" + participant + "/vspt",
                    encode_vspt(entries))
    if "apct" in platform:
        data += key("/shared/tables/apct", encode_apct(platform["apct"]))
    if "apat" in platform:
        data += key("/shared/tables/apat", encode_apat(platform["apat"]))
    for name, entries in platform.get("itmt", {}).items():
        data += key("/shared/tables/itmt/" + name, itmt_v1(entries))
    if "idsp" in platform:
        data += key("/participants/IETM/idsp", encode_idsp(platform["idsp"]))
    for k, value in platform.get("raw", {}).items():
        data += key(k, value)     # arbitrary (e.g. malformed) items
    return data
