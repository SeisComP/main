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
class TestAvailability(FDSNWSTest):
    def __init__(self, port=9980):
        super().__init__(port)
        self.extraArgs.append("--useArclinkAccess=true")

    # --------------------------------------------------------------------------
    def test(self):
        print("Testing availability service")

        query = f"{self.url}/availability/1/"
        ctTXT = "text/plain; charset=utf-8"
        ctCSV = "text/csv; charset=utf-8"
        ctJSON = "application/json; charset=utf-8"
        resFile = f"{self.rootdir}/results/availability-{{}}.txt"

        i = 1
        # (contentType, URL, ignoreChars, concurrent)
        tests = [
            ("extent", ctTXT, [], False),
            ("extent?merge=samplerate", ctTXT, [], False),
            ("extent?merge=quality", ctTXT, [], True),
            ("extent?merge=quality,samplerate", ctTXT, [], False),
            (
                "extent?starttime=2019-08-20T00:00:00&orderby=latestupdate",
                ctTXT,
                [],
                False,
            ),
            (
                "extent?network=AM&channel=HDF&orderby=timespancount_desc",
                ctTXT,
                [],
                False,
            ),
            (
                "extentauth?station=R0F05&orderby=latestupdate_desc&"
                "includerestricted=true&format=geocsv",
                ctCSV,
                [],
                False,
            ),
            ("extent?orderby=latestupdate_desc&format=request", ctTXT, [], False),
            (
                "extentauth?orderby=latestupdate_desc&includerestricted=true&"
                "format=json&merge=quality",
                ctJSON,
                [(12, 32)],
                True,
            ),
            ("query?net=AM", ctTXT, [], False),
            ("query?net=AM&mergegaps=10.0", ctTXT, [], False),
            ("query?net=AM&mergegaps=10.0&merge=overlap", ctTXT, [], False),
            ("query?net=AM&mergegaps=10.0&merge=overlap,samplerate", ctTXT, [], True),
            (
                "query?net=AM&mergegaps=10.0&merge=overlap,samplerate,quality",
                ctTXT,
                [],
                False,
            ),
            ("query?net=AM&show=latestupdate&limit=3", ctTXT, [], False),
            ("query?net=AM&format=geocsv&show=latestupdate", ctCSV, [], False),
            ("query?net=AM&format=json&show=latestupdate", ctJSON, [(12, 32)], False),
            (
                "query?net=AM&channel=HDF&format=json&merge=quality,samplerate,overlap&"
                "latestupdate",
                ctJSON,
                [(12, 32)],
                False,
            ),
            ("query?net=AM&format=request", ctTXT, [], False),
            ("queryauth?net=AM&station=R0F05&includerestricted=true", ctTXT, [], True),
        ]
        for q, ct, ignoreRanges, concurrent in tests:
            auth = q.startswith("queryauth") or q.startswith("extentauth")
            self.testHTTP(
                f"{query}{q}",
                ct,
                ignoreRanges,
                concurrent,
                auth=auth,
                dataFile=resFile.format(i),
                testID=i,
            )
            i += 1


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestAvailability()
    sys.exit(app())


# vim: ts=4 et tw=88
