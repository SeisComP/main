from __future__ import absolute_import, division, print_function

from fdsnwstest import FDSNWSTest


###############################################################################
class TestStationBase(FDSNWSTest):
    CT_TXT = "text/plain"
    CT_XML = "application/xml"

    def query(self):
        return "{}/station/1/query".format(self.url)


# vim: ts=4 et tw=79
