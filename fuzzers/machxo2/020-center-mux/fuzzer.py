from collections import defaultdict

from fuzzconfig import FuzzConfig
import interconnect
import nets
import pytrellis
import re
import argparse
from mk_nets import *

# The relevant tiles were inferred from the center_mux experiment.
jobs = [
        {
            "netnames" : eclk_out + eclk_cib + eclk_div,
            "cfg" : FuzzConfig(job="GLOBAL_FIXED", family="MachXO2", device="LCMXO2-1200HC", ncl="center-mux.ncl",
                      tiles=["CENTER6:CENTER_EBR_CIB", "CENTER9:CENTER8", "CENTER8:CENTER7", "CENTER7:CENTER6",
                             "CENTER5:CENTER5", "CENTER4:CENTER4"]),
            "prefix" : "1200_"
        }
]


def main(args):
    pytrellis.load_database("../../../database")

    for job in [jobs[i] for i in args.ids]:
        cfg = job["cfg"]
        cfg.setup()
        interconnect.fuzz_interconnect_with_netnames(config=cfg, netnames=job["netnames"],
                                                     netname_filter_union=False,
                                                     netdir_override=defaultdict(lambda : str("sink")),
                                                     nonlocal_prefix=job["prefix"])

    # TODO: R6C13_JA0 --> R6C13_JCE0_DCC. But TCL also claims
    # R6C13_CLKI0_DCC --> R6C13_CLKO0_DCC (pseudo = 1). Contradiction?
    # From talking to Dave: No it's not a contradiction. A
    # config bit controls whether JCE0 has any effect.
    # Might be fuzzed with something like:
    # interconnect.fuzz_interconnect_with_netnames(config=cfg, netnames=["R6C13_CLKI0_DCC", "R6C13_CLKO0_DCC", "R6C13_JCE0_DCC"],
    #                                              netname_filter_union=False,
    #                                              netdir_override = {
    #                                                 "R6C13_JCE0_DCC" : "sink",
    #                                              })


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Center Mux Routing Fuzzer.")
    parser.add_argument(dest="ids", metavar="N", type=int, nargs="*",
                    default=range(0, len(jobs)), help="Job (indices) to run.")
    args = parser.parse_args()
    main(args)
