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
import seiscomp.client, seiscomp.datamodel, seiscomp.io


class ObjectDumper(seiscomp.client.Application):

    def __init__(self):
        seiscomp.client.Application.__init__(self, len(sys.argv), sys.argv)
        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, False)
        self.setMessagingUsername("")

    def createCommandLineDescription(self):
        seiscomp.client.Application.createCommandLineDescription(self)
        self.commandline().addGroup("Dump")
        self.commandline().addStringOption("Dump", "public-id,P", "publicID")

    def loadEventParametersObject(self, publicID):
        for tp in \
                seiscomp.datamodel.Pick, seiscomp.datamodel.Amplitude, seiscomp.datamodel.Origin, \
                seiscomp.datamodel.Event, seiscomp.datamodel.FocalMechanism, \
                seiscomp.datamodel.Magnitude, seiscomp.datamodel.StationMagnitude:

            obj = self.query().loadObject(tp.TypeInfo(), publicID)
            obj = tp.Cast(obj)
            if obj:
                ep = seiscomp.datamodel.EventParameters()
                ep.add(obj)
                return ep

    def loadInventoryObject(self, publicID):
        for tp in \
                seiscomp.datamodel.Network, seiscomp.datamodel.Station, seiscomp.datamodel.Sensor, \
                seiscomp.datamodel.SensorLocation, seiscomp.datamodel.Stream:

            obj = self.query().loadObject(tp.TypeInfo(), publicID)
            obj = tp.Cast(obj)
            if obj:
                return obj

    def run(self):
        publicID = self.commandline().optionString("public-id")
        obj = self.loadEventParametersObject(publicID)
        if obj is None:
            obj = self.loadInventoryObject(publicID)
        if obj is None:
            raise ValueError("unknown object '" + publicID + "'")

        # dump formatted XML archive to stdout
        ar = seiscomp.io.XMLArchive()
        ar.setFormattedOutput(True)
        ar.create("-")
        ar.writeObject(obj)
        ar.close()
        return True


if __name__ == "__main__":
    app = ObjectDumper()
    app()
