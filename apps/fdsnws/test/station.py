from fdsnwstest import FDSNWSTest


###############################################################################
class TestStationBase(FDSNWSTest):
    CT_TXT = "text/plain; charset=utf-8"
    CT_XML = "application/xml; charset=utf-8"

    def query(self):
        return f"{self.url}/station/1/query"


# vim: ts=4 et tw=88
