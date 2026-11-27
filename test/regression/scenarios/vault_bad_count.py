"""APCT v2 claiming 0xffffffff conditions in a set (MAX_APCT_COUNT check).

thermald must refuse the table instead of allocating or reading that
many conditions, keep running and exit cleanly.
"""

import gddv
from platforms import base_vault, check_rejected_cleanly, rapl

PLATFORM = {
    "zones": ["SEN2"],
    "initial_temps": {"SEN2": 35},
    "rapl": rapl(),
    "gddv": base_vault(raw={
        "/shared/tables/apct": gddv.u64(2) + gddv.u64(1)
        + gddv.u64(0xffffffff) + gddv.u64(gddv.DEFAULT),
    }),
}


def run(g):
    g.wait_ready(timeout=40)
    g.check("exceeds maximum" in g.log(), "APCT count not rejected")
    check_rejected_cleanly(g)
