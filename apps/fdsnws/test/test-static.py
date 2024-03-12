#!/usr/bin/env python3

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import sys

from fdsnwstest import FDSNWSTest
from seiscomp.fdsnws import event, station, dataselect, availability


###############################################################################
class TestStatic(FDSNWSTest):
    # --------------------------------------------------------------------------
    def test(self):
        print("Testing static files")

        ctTXT = "text/plain; charset=utf-8"
        ctXML = "application/xml; charset=utf-8"
        share = f"{self.sharedir}/"

        # main url
        self.testHTTP(self.url)

        # event
        self.testHTTP(f"{self.url}/event")
        self.testHTTP(f"{self.url}/event/1", dataFile=f"{share}event.html")
        self.testHTTP(f"{self.url}/event/1/version", ctTXT, data=event.VERSION)
        self.testHTTP(
            f"{self.url}/event/1/application.wadl",
            ctXML,
            dataFile=f"{share}event.wadl",
        )
        self.testHTTP(
            f"{self.url}/event/1/catalogs",
            ctXML,
            dataFile=f"{share}catalogs.xml",
        )
        self.testHTTP(
            f"{self.url}/event/1/contributors",
            ctXML,
            dataFile=f"{share}contributors.xml",
        )
        self.testHTTP(
            f"{self.url}/event/1/builder", dataFile=f"{share}event-builder.html"
        )

        # station
        self.testHTTP(f"{self.url}/station")
        self.testHTTP(f"{self.url}/station/1", dataFile=f"{share}station.html")
        self.testHTTP(f"{self.url}/station/1/version", ctTXT, data=station.VERSION)
        self.testHTTP(
            f"{self.url}/station/1/application.wadl",
            ctXML,
        )  # dataFile=share+'station.wadl')
        self.testHTTP(
            f"{self.url}/station/1/builder",
            dataFile=f"{share}station-builder.html",
        )

        # dataselect
        self.testHTTP(f"{self.url}/dataselect")
        self.testHTTP(f"{self.url}/dataselect/1", dataFile=f"{share}dataselect.html")
        self.testHTTP(
            f"{self.url}/dataselect/1/version", ctTXT, data=dataselect.VERSION
        )
        self.testHTTP(
            f"{self.url}/dataselect/1/application.wadl",
            ctXML,
            dataFile=f"{share}dataselect.wadl",
        )
        self.testHTTP(
            f"{self.url}/dataselect/1/builder",
            dataFile=f"{share}dataselect-builder.html",
        )

        # availability
        self.testHTTP(f"{self.url}/availability")
        self.testHTTP(
            f"{self.url}/availability/1", dataFile=f"{share}availability.html"
        )
        self.testHTTP(
            f"{self.url}/availability/1/version",
            ctTXT,
            data=availability.VERSION,
        )
        self.testHTTP(
            f"{self.url}/availability/1/application.wadl",
            ctXML,
            dataFile=f"{share}availability.wadl",
        )
        self.testHTTP(
            f"{self.url}/availability/1/builder-query",
            dataFile=f"{share}availability-builder-query.html",
        )
        self.testHTTP(
            f"{self.url}/availability/1/builder-extent",
            dataFile=f"{share}availability-builder-extent.html",
        )


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestStatic()
    sys.exit(app())


# vim: ts=4 et tw=88
