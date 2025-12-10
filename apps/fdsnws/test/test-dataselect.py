#!/usr/bin/env python3

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import sys

from fdsnwstest import FDSNWSTest

import seiscomp.io
import seiscomp.core


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

        self.testRecordStream()

    def testRecordStream(self):
        url = f"fdsnws://localhost:{self.port}"
        rs = seiscomp.io.RecordStream.Open(url)
        if rs is None:
            raise ValueError(f"Unable to open recordstream '{url}'")

        if not rs.setRecordType("mseed"):
            raise ValueError(f"Format 'mseed' is not supported by recordstream '{url}'")

        start = seiscomp.core.Time(2019, 8, 2, 17, 59, 59)
        end = seiscomp.core.Time(2019, 8, 2, 18, 00, 59)
        rs.addStream("AM", "R187C", "00", "EHZ", start, end)

        inputRecord = seiscomp.io.RecordInput(
            rs, seiscomp.core.Array.INT, seiscomp.core.Record.SAVE_RAW
        )

        result = bytearray()
        for rec in inputRecord:
            result.extend(rec.raw().str())

        dataFile = f"{self.rootdir}/results/dataselect-6.mseed"
        with open(dataFile, "rb") as f:
            expected = f.read()

        errPos, errMsg = self.diff(expected, result, None)
        if errPos is not None:
            raise ValueError(f"Unexpected content at byte {errPos}: {errMsg}")


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestDataSelect()
    sys.exit(app())


# vim: ts=4 et tw=88
