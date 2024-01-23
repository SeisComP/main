from fdsnwstest import FDSNWSTest


###############################################################################
class TestStationBase(FDSNWSTest):
    CT_TXT = "text/plain"
    CT_XML = "application/xml"

    def query(self):
        return f"{self.url}/station/1/query"


# vim: ts=4 et tw=88
