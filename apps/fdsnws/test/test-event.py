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
class TestEvent(FDSNWSTest):
    def __init__(self, port=9980):
        super().__init__(port)
        self.extraArgs.append("--hideStationMagnitudes=false")

    # --------------------------------------------------------------------------
    def test(self):
        print("Testing event service")

        query = f"{self.url}/event/1/query"
        ctTXT = "text/plain; charset=utf-8"
        ctXML = "application/xml; charset=utf-8"
        resFile = f"{self.rootdir}/results/event-"

        # updatedafter
        for n, param in enumerate(
            (
                # rs2019qrwzhx is selected only through its preferred magnitude
                "updatedafter=2019-08-25T08:55:00",
                # gfz2021jtmw was last modified at 2021-05-19T19:18:25.377184
                "updatedafter=2021-05-19T19:18:25.377184",
                "updatedafter=2021-05-19T19:18:25.377183",
            ),
            1,
        ):
            self.testHTTP(
                f"{query}?format=text&{param}",
                ctTXT,
                dataFile=f"{resFile}updatedafter-{n}.txt",
                testID=f"updatedafter-{n}",
            )

        i = 1
        tests = [
            ("?format=text", ctTXT, False),
            ("?format=text&minmag=3&mindepth=20&minlon=150", ctTXT, False),
            ("?format=text&eventtype=earthquake,unknown", ctTXT, False),
            ("?format=sc3ml&eventid=rs2019qsodmc&formatted=true", ctXML, True),
            (
                "?format=sc3ml&includeallorigins=true&includeallmagnitudes=true"
                "&includearrivals=true&includefocalmechanism=true&"
                "includestationmts=true&includecomments=true&eventid=rs2019qsodmc&"
                "formatted=true",
                ctXML,
                False,
            ),
            ("", ctXML, False),
            ("?format=qml-rt", ctXML, False),
            ("?format=csv", ctTXT, False),
            # Mw is found in derived origin of preferred focal mechanism which is not
            # the preferred origin
            ("?format=text&magnitudetype=Mw", ctTXT, False),
            ("?format=text&minlat=-30&maxlat=30&minlon=150&maxlon=-150", ctTXT, False),
            # date line crossing with a longitude of 0
            ("?format=text&minlon=160&maxlon=0", ctTXT, False),
            # event close to the longitude limit of the circle's bounding box
            ("?format=text&lat=-30&lon=-50&maxradius=50", ctTXT, False),
            # event (almost) in the center of the circle
            (
                "?format=text&lat=-7.916882046&lon=159.24310297&maxradius=0.001",
                ctTXT,
                False,
            ),
        ]

        # station magnitudes, magnitude contributions and amplitudes
        self.testHTTP(
            f"{query}?format=sc3ml&eventid=rs2019qsodmc&includestationmagnitudes=true"
            "&formatted=true",
            ctXML,
            dataFile=f"{resFile}stamags-1.txt",
            testID="stamags-1",
        )
        for n, (fmt, param) in enumerate(
            (
                ("sc3ml", "includeallmagnitudes=true"),
                ("qml", "includeallmagnitudes=true&includearrivals=true"),
            ),
            2,
        ):
            self.testHTTP(
                f"{query}?format={fmt}&eventid=gfz2021jtmw"
                f"&includestationmagnitudes=true&{param}&formatted=true",
                ctXML,
                dataFile=f"{resFile}stamags-{n}.txt",
                testID=f"stamags-{n}",
            )

        for q, ct, concurrent in tests:
            self.testHTTP(
                f"{query}{q}",
                ct,
                [],
                concurrent,
                dataFile=f"{resFile}{i}.txt",
                testID=i,
            )
            i += 1

        # a maximum depth of 0 is a valid filter
        self.testHTTP(f"{query}?format=text&maxdepth=0", None, retCode=204, testID=i)


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestEvent()
    sys.exit(app())


# vim: ts=4 et tw=88
