#!/usr/bin/env python3

import sys
import time

from twisted.web.http import datetimeToString, stringToDatetime

from station import TestStationBase


###############################################################################
class TestStationConditionalRequests(TestStationBase):
    def __init__(self, port=9980):
        super().__init__(port)
        self.extraArgs.append("--handleConditionalRequests=true")

    def test(self):
        def validateDateString(dateString):
            try:
                _ = stringToDatetime(dateString)
            except ValueError as e:
                raise ValueError(f"Invalid date string: {dateString!r}") from e
            return True

        def createQueryString(
            respFormat, level, net=None, sta=None, loc=None, cha=None
        ):
            # pylint: disable=R0913
            d = {"level": level, "format": respFormat}
            if net:
                d["network"] = net
            if sta:
                d["station"] = sta
            if loc:
                d["location"] = loc
            if cha:
                d["channel"] = cha

            return "{}?{}".format(
                self.query(), "&".join("{}={}".format(k, v) for k, v in d.items())
            )

        def createQueryStrings(respFormat, levels, **kwargs):
            return [createQueryString(respFormat, level, **kwargs) for level in levels]

        def createTestCases(
            respFormat,
            levels,
            ifModifiedSince,
            contentType=None,
            dataFile=None,
            retCode=200,
            respHeaders=None,
            **kwargs,
        ):
            # pylint: disable=R0913
            return [
                (
                    qs,
                    contentType,
                    retCode,
                    {"If-Modified-Since": datetimeToString(ifModifiedSince)},
                    respHeaders,
                    dataFile,
                )
                for qs in createQueryStrings(respFormat, levels, **kwargs)
            ]

        print("Testing station service (conditional requests)")

        epoch = 0.0
        inOneYear = time.time() + 365 * 24 * 60 * 60
        formatText = "text"
        formatXML = "xml"
        formatSCML = "sc3ml"
        levelsText = ["network", "station", "channel"]
        levelsXML = levelsText[:] + ["response"]

        tests = []
        # 200
        sncl = {"net": "AM", "sta": "R187C", "loc": "00", "cha": "EHZ"}
        respHeaders = {"last-modified": validateDateString}
        tests.extend(
            createTestCases(
                formatText,
                ["station"],
                epoch,
                respHeaders=respHeaders,
                dataFile=self._dataFile(1),
                **sncl,
            )
        )
        tests.extend(
            createTestCases(
                formatSCML,
                ["station"],
                epoch,
                respHeaders=respHeaders,
                dataFile=self._dataFile(2),
                **sncl,
            )
        )

        # 204
        tests.extend(
            createTestCases(
                formatText,
                levelsText,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_TXT,
                net="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatText,
                levelsText,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_TXT,
                sta="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatText,
                levelsText,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_TXT,
                loc="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatText,
                levelsText,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_TXT,
                cha="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatXML,
                levelsXML,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_XML,
                net="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatXML,
                levelsXML,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_XML,
                sta="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatXML,
                levelsXML,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_XML,
                loc="FOO",
            )
        )
        tests.extend(
            createTestCases(
                formatXML,
                levelsXML,
                inOneYear,
                retCode=204,
                contentType=TestStationBase.CT_XML,
                cha="FOO",
            )
        )
        # 304
        tests.extend(
            createTestCases(formatText, levelsText, inOneYear, retCode=304, net="AM")
        )
        tests.extend(
            createTestCases(formatText, levelsText, inOneYear, retCode=304, sta="R187C")
        )
        tests.extend(
            createTestCases(formatText, levelsText, inOneYear, retCode=304, loc="00")
        )
        tests.extend(
            createTestCases(formatText, levelsText, inOneYear, retCode=304, cha="EHZ")
        )
        tests.extend(
            createTestCases(formatXML, levelsXML, inOneYear, retCode=304, net="AM")
        )
        tests.extend(
            createTestCases(formatXML, levelsXML, inOneYear, retCode=304, sta="R187C")
        )
        tests.extend(
            createTestCases(formatXML, levelsXML, inOneYear, retCode=304, loc="00")
        )
        tests.extend(
            createTestCases(formatXML, levelsXML, inOneYear, retCode=304, cha="EHZ")
        )

        for i, testConfig in enumerate(tests, start=1):
            qs, ct, retCode, reqHeaders, respHeaders, dataFile = testConfig
            self.testHTTP(
                qs,
                ct,
                reqHeaders=reqHeaders,
                respHeaders=respHeaders,
                dataFile=dataFile,
                retCode=retCode,
                testID=i,
            )

    def _dataFile(self, i):
        return f"{self.rootdir}/results/station-conditional-{i}.txt"


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestStationConditionalRequests()
    sys.exit(app())


# vim: ts=4 et tw=88
