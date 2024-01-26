#!/usr/bin/env python3

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import sys

from station import TestStationBase


###############################################################################
class TestStationPrecision(TestStationBase):
    def __init__(self, port=9980):
        super().__init__(port)
        self.extraArgs.append("--inventoryCoordinatePrecision=2")

    # --------------------------------------------------------------------------
    def test(self):
        print("Testing station service (limited precision)")

        query = self.query()
        resFile = f"{self.rootdir}/results/station-precision-"

        i = 1
        tests = [
            ("?format=text&level=channel", TestStationBase.CT_TXT, [], False),
        ]
        for q, ct, ignoreRanges, concurrent in tests:
            self.testHTTP(
                f"{query}{q}",
                ct,
                ignoreRanges,
                concurrent,
                dataFile=f"{resFile}{i}.txt",
                testID=i,
            )
            i += 1


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestStationPrecision()
    sys.exit(app())


# vim: ts=4 et tw=88
