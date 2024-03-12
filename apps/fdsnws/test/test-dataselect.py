#!/usr/bin/env python3

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import sys

from fdsnwstest import FDSNWSTest


###############################################################################
class TestDataSelect(FDSNWSTest):
    # --------------------------------------------------------------------------
    def test(self):
        print("Testing dataselect service")

        query = f"{self.url}/dataselect/1/query"
        ctTXT = "text/plain; charset=utf-8"
        ctMSeed = "application/vnd.fdsn.mseed"
        resFile = f"{self.rootdir}/results/dataselect-{{}}.mseed"

        i = 1
        self.testHTTP(f"{query}?station=R0F05", ctTXT, retCode=403, testID=i)
        i += 1
        tests = [
            ("?channel=EHZ", False),
            (
                "?net=AM&sta=R187C&loc=00&cha=EHZ&starttime=2019-08-02T18:00:30&"
                "endtime=2019-08-02T18:00:40",
                False,
            ),
            ("auth?network=AM&station=R0F05&starttime=2019-08-02T12:00:00", True),
        ]
        for q, concurrent in tests:
            self.testHTTP(
                f"{query}{q}",
                ctMSeed,
                [],
                concurrent,
                dataFile=resFile.format(i),
                testID=i,
                auth=q.startswith("auth"),
            )
            i += 1


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestDataSelect()
    sys.exit(app())


# vim: ts=4 et tw=88
