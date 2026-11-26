"""Write seed vaults for the GDDV fuzzer: one per table type and a mix.

    python3 make_seeds.py <dir>
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "regression", "lib"))
import gddv  # noqa: E402

PPCC = gddv.ppcc(10000, 35000, 28000, 32000, 500, 35000, 64000, 2, 2, 1000)
PSV = gddv.psv(gddv.TCPU, "\\_SB_.SEN2", 50)

SEEDS = {
    "psvt": {"psvt": {"IETM.D0": [PSV, gddv.psv(gddv.TCPU, "\\_SB_.SEN3", 60,
                                                limit=20000)]}},
    "ppcc": {"ppcc": {"TCPU.D0": PPCC}},
    "ppcc_pl1_only": {"ppcc_nr_limits": 1, "ppcc": {"TCPU.D0": PPCC}},
    "apct_apat": {
        "apct": [(2, [gddv.cond(gddv.OEM0, gddv.EQUAL, 5),
                      gddv.temp_cond("\\_SB_.SEN2", gddv.GREATER_OR_EQUAL, 50)]),
                 (1, [gddv.cond(gddv.DEFAULT)])],
        "apat": [gddv.action(1, "AC", gddv.TCPU, "PL1MAX", 30000),
                 gddv.action(2, "Hot", gddv.IETM, "PSVT", "IETM.D0")]},
    "vsensors": {"vsct": {"VR1.D0": [("\\_SB_.SEN3", 1000, 1000)],
                          "VR2.D0": [("\\_SB_.SEN4", 500, 250)]},
                 "vspt": {"VR1.D0": [(0, 1)], "VR2.D0": [(60, 2)]}},
    "itmt_v1_short": {"raw": {"/shared/export/itmt": gddv.itmt_v1(
        [("\\_SB_.SEN1", 62, 28000, 35000)], short_last=True)}},
}
SEEDS["mixed"] = {k: v for seed in SEEDS.values() for k, v in seed.items()}


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    for name, tables in SEEDS.items():
        with open(os.path.join(outdir, "seed_" + name + ".bin"), "wb") as f:
            f.write(gddv.encode(tables))


if __name__ == "__main__":
    main()
