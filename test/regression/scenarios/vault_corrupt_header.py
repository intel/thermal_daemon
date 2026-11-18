"""A data vault that isn't one: garbage, and a valid header whose first
item claims a key longer than the vault.

thermald must not crash; adaptive mode fails and it keeps running.
"""

import struct

import gddv
from platforms import check_rejected_cleanly, rapl

garbage = bytes((i * 37 + 11) & 0xff for i in range(200))
huge_key = gddv.HEADER + struct.pack("<II", 0, 0x7ffffff0) + b"/shared"

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    # The vault has to be one buffer: test the huge key length, and
    # append the garbage behind it so neither can be parsed
    "gddv": {"raw_vault": huge_key + garbage},
}


def run(g):
    g.wait_ready(timeout=40)
    check_rejected_cleanly(g)
