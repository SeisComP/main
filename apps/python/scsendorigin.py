#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) GFZ Potsdam                                                #
# All rights reserved.                                                     #
#                                                                          #
# GNU Affero General Public License Usage                                  #
# This file may be used under the terms of the GNU Affero                  #
# Public License version 3.0 as published by the Free Software Foundation  #
# and appearing in the file LICENSE included in the packaging of this      #
# file. Please review the following information to ensure the GNU Affero   #
# Public License version 3.0 requirements will be met:                     #
# https://www.gnu.org/licenses/agpl-3.0.html.                              #
############################################################################

import sys
import seiscomp.core
import seiscomp.datamodel
import seiscomp.client
import seiscomp.logging


class SendOrigin(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setDatabaseEnabled(False, False)
        self.setMessagingEnabled(True)
        self.setPrimaryMessagingGroup("GUI")

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            cstr = self.commandline().optionString("coord")
            tstr = self.commandline().optionString("time")
        except:
            sys.stderr.write(
                "Must specify origin using '--coord lat,lon,dep --time time'\n")
            return False

        self.origin = seiscomp.datamodel.Origin.Create()

        ci = seiscomp.datamodel.CreationInfo()
        ci.setAgencyID(self.agencyID())
        ci.setCreationTime(seiscomp.core.Time.GMT())
        self.origin.setCreationInfo(ci)

        lat, lon, dep = list(map(float, cstr.split(",")))
        self.origin.setLongitude(seiscomp.datamodel.RealQuantity(lon))
        self.origin.setLatitude(seiscomp.datamodel.RealQuantity(lat))
        self.origin.setDepth(seiscomp.datamodel.RealQuantity(dep))

        time = seiscomp.core.Time()
        time.fromString(tstr.replace("/", "-") + ":0:0", "%F %T")
        self.origin.setTime(seiscomp.datamodel.TimeQuantity(time))

        return True

    def createCommandLineDescription(self):
        try:
            self.commandline().addGroup("Parameters")
            self.commandline().addStringOption("Parameters",
                                               "coord",
                                               "Latitude,longitude,depth of origin")
            self.commandline().addStringOption("Parameters",
                                               "time", "time of origin")
        except:
            seiscomp.logging.warning("caught unexpected error %s" % sys.exc_info())

    def printUsage(self):
        print('''Usage:
  scsendorigin [options]

Create an artificial origin and send to the messaging''')

        seiscomp.client.Application.printUsage(self)

        print('''Examples:
Send an artificial origin with hypocenter parameters to the messaging
  scsendorigin --time "2022-05-01 10:00:00" --coord 52,12,10
''')

    def run(self):
        msg = seiscomp.datamodel.ArtificialOriginMessage(self.origin)
        self.connection().send(msg)
        return True


app = SendOrigin(len(sys.argv), sys.argv)
# app.setName("scsendorigin")
app.setMessagingUsername("scsendorg")
sys.exit(app())
