#!/usr/bin/env python

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

from __future__ import absolute_import, division, print_function

import sys

from fdsnwstest import FDSNWSTest
from seiscomp.fdsnws import event, station, dataselect, availability


###############################################################################
class TestStatic(FDSNWSTest):

    #--------------------------------------------------------------------------
    def test(self):
        print('Testing static files')

        ctTXT = 'text/plain'
        ctXML = 'application/xml'
        share = self.sharedir + '/'

        # main url
        self.testHTTP(self.url)

        # event
        self.testHTTP('{}/event'.format(self.url))
        self.testHTTP('{}/event/1'.format(self.url),
                      dataFile=share+'event.html')
        self.testHTTP('{}/event/1/version'.format(self.url), ctTXT,
                      data=event.VERSION)
        self.testHTTP('{}/event/1/application.wadl'.format(self.url), ctXML,
                      dataFile=share+'event.wadl')
        self.testHTTP('{}/event/1/catalogs'.format(self.url), ctXML,
                      dataFile=share+'catalogs.xml')
        self.testHTTP('{}/event/1/contributors'.format(self.url), ctXML,
                      dataFile=share+'contributors.xml')
        self.testHTTP('{}/event/1/builder'.format(self.url),
                      dataFile=share+'event-builder.html')

        # station
        self.testHTTP('{}/station'.format(self.url))
        self.testHTTP('{}/station/1'.format(self.url),
                      dataFile=share+'station.html')
        self.testHTTP('{}/station/1/version'.format(self.url), ctTXT,
                      data=station.VERSION)
        self.testHTTP('{}/station/1/application.wadl'.format(self.url), ctXML,
                      )#dataFile=share+'station.wadl')
        self.testHTTP('{}/station/1/builder'.format(self.url),
                      dataFile=share+'station-builder.html')

        # dataselect
        self.testHTTP('{}/dataselect'.format(self.url))
        self.testHTTP('{}/dataselect/1'.format(self.url),
                      dataFile=share+'dataselect.html')
        self.testHTTP('{}/dataselect/1/version'.format(self.url), ctTXT,
                      data=dataselect.VERSION)
        self.testHTTP('{}/dataselect/1/application.wadl'.format(self.url),
                      ctXML, dataFile=share+'dataselect.wadl')
        self.testHTTP('{}/dataselect/1/builder'.format(self.url),
                      dataFile=share+'dataselect-builder.html')

        # availability
        self.testHTTP('{}/availability'.format(self.url))
        self.testHTTP('{}/availability/1'.format(self.url),
                      dataFile=share+'availability.html')
        self.testHTTP('{}/availability/1/version'.format(self.url), ctTXT,
                      data=availability.VERSION)
        self.testHTTP('{}/availability/1/application.wadl'.format(self.url),
                      ctXML, dataFile=share+'availability.wadl')
        self.testHTTP('{}/availability/1/builder-query'.format(self.url),
                      dataFile=share+'availability-builder-query.html')
        self.testHTTP('{}/availability/1/builder-extent'.format(self.url),
                      dataFile=share+'availability-builder-extent.html')


#------------------------------------------------------------------------------
if __name__ == '__main__':
    app = TestStatic()
    sys.exit(app())



# vim: ts=4 et tw=79
