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

import traceback
import sys
import seiscomp.core, seiscomp.client, seiscomp.datamodel


class OriginList(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = None
        self._endTime = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Origins")
        self.commandline().addStringOption("Origins", "begin",
                                           "specify the lower bound of the time interval. Time format: '1970-01-01 00:00:00'")
        self.commandline().addStringOption("Origins", "end",
                                           "specify the upper bound of the time interval. Time format: '1970-01-01 00:00:00'")
        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            start = self.commandline().optionString("begin")
            self._startTime = seiscomp.core.Time()
            if self._startTime.fromString(start, "%F %T") == False:
                sys.stderr.write(
                    "Wrong 'begin' format '%s' -> setting to None\n" % start)
        except:
            sys.stderr.write("Wrong 'begin' format -> setting to None\n")
            self._startTime = seiscomp.core.Time()

#   sys.stderr.write("Setting start to %s\n" % self._startTime.toString("%F %T"))

        try:
            end = self.commandline().optionString("end")
            self._endTime = seiscomp.core.Time.FromString(end, "%F %T")
        except:
            self._endTime = seiscomp.core.Time.GMT()

#   sys.stderr.write("Setting end to %s\n" % self._endTime.toString("%F %T"))

        return True

    def run(self):
        q = "select PublicObject.%s, Origin.* from Origin, PublicObject where Origin._oid=PublicObject._oid and Origin.%s >= '%s' and Origin.%s < '%s'" %\
            (self.database().convertColumnName("publicID"),
             self.database().convertColumnName("time_value"),
             self.database().timeToString(self._startTime),
             self.database().convertColumnName("time_value"),
             self.database().timeToString(self._endTime))

        for obj in self.query().getObjectIterator(q, seiscomp.datamodel.Origin.TypeInfo()):
            org = seiscomp.datamodel.Origin.Cast(obj)
            if org:
                sys.stdout.write("%s\n" % org.publicID())

        return True


try:
    app = OriginList(len(sys.argv), sys.argv)
    rc = app()
except:
    sys.stderr.write("%s\n" % traceback.format_exc())
    sys.exit(1)

sys.exit(rc)
