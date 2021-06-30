#!/usr/bin/env python

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

from __future__ import absolute_import, division, print_function

import sys

from station import TestStationBase


###############################################################################
class TestStation(TestStationBase):

    #--------------------------------------------------------------------------
    def test(self):
        print('Testing station service')

        query = self.query()
        resFile = self.rootdir + '/results/station-'

        i = 1
        tests = [
            ('?format=text&level=channel', TestStationBase.CT_TXT, [], False),
            ('?format=text&includerestricted=false', TestStationBase.CT_TXT, [], True),
            ('?format=text&startbefore=2019-07-01', TestStationBase.CT_TXT, [], False),
            ('?level=channel&includeavailability=true', TestStationBase.CT_XML, [(172, 198, 7, 0)], False),
            ('?format=sc3ml&network=AM&station=R0F05&location=00&channel=SHZ&latitude=52&longitude=13&maxradius=0.5&level=response&includeavailability=true', TestStationBase.CT_XML, [], True),
            ('?network=DK&station=OVD', TestStationBase.CT_XML, [(172, 198, 7)], False)
        ]
        for q, ct, ignoreRanges, concurrent in tests:
            self.testHTTP('{}{}'.format(query, q), ct, ignoreRanges, concurrent,
                          dataFile='{}{}.txt'.format(resFile, i), testID=i)
            i += 1

        # POST tests
        tests = [
            ('', TestStationBase.CT_TXT, [], False),
            ('', TestStationBase.CT_TXT, [], False)
        ]
        postFile = "{}/post/station-{}.txt"
        for q, ct, ignoreRanges, concurrent in tests:
            with open(postFile.format(self.rootdir, i), 'rb') as f:
                self.testHTTP(query,
                              ct, ignoreRanges, concurrent,
                              dataFile='{}{}.txt'.format(resFile, i), testID=i,
                              postData=f)
            i += 1


#------------------------------------------------------------------------------
if __name__ == '__main__':
    app = TestStation()
    sys.exit(app())



# vim: ts=4 et tw=79
