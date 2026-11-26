"""Malformed tables in an otherwise valid vault:

  - an APCT cut off in the middle of a condition
  - an APAT whose last field claims a string longer than the buffer

thermald must reject them without crashing or hanging and exit cleanly.
"""

import gddv
from platforms import base_vault, check_rejected_cleanly, rapl

good_apct = gddv.encode_apct([
    (1, [gddv.cond(gddv.OEM0, gddv.EQUAL, 1),
         gddv.cond(gddv.POWER_SLIDER, gddv.EQUAL, 75)]),
    (2, [gddv.cond(gddv.DEFAULT)]),
])
bad_apat = (gddv.u64(2) + gddv.u64(1) + gddv.string("T")
            + gddv.string(gddv.TCPU) + gddv.u64(0) + gddv.string("PL1MAX")
            # string object claiming 4096 bytes, followed by 3
            + gddv.struct.pack("<IQ", gddv.TYPE_STRING, 4096) + b"200")

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "odvp": [1],
    "gddv": base_vault(raw={
        "/shared/tables/apct": good_apct[:len(good_apct) // 2 + 5],
        "/shared/tables/apat": bad_apat,
    }),
}


def run(g):
    g.wait_ready(timeout=40)
    check_rejected_cleanly(g)
