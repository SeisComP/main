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

import sys, re
import seiscomp.core, seiscomp.client, seiscomp.io, seiscomp.datamodel


class WfqQuery(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)
        self.addMessagingSubscription("QC")
        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setAutoApplyNotifierEnabled(False)
        self.setInterpretNotifierEnabled(True)

    def createCommandLineDescription(self):
        self.commandline().addGroup("Output")
        self.commandline().addStringOption("Output", "filename,o", "xml output filename")
        self.commandline().addOption("Output", "formatted,f", "use formatted xml output")

        self.commandline().addGroup("Query")
        self.commandline().addStringOption("Query", "streamID,w",
                                           "waveform stream ID: <networkCode>.<stationCode>.<locationCode>.<channelCode>")
        self.commandline().addStringOption("Query", "parameter,p",
                                           "QC parameter: (e.g. LATENCY, DELAY, ...)")
        self.commandline().addStringOption(
            "Query", "startTime,b", "start time: YYYY-MM-DD hh:mm:ss")
        self.commandline().addStringOption(
            "Query", "endTime,e", "end time: YYYY-MM-DD hh:mm:ss")

        return True

    def validateParameters(self):
        if self.commandline().hasOption("filename"):
            self._outfile = self.commandline().optionString("filename")
        else:
            sys.stderr.write("Please specify the xml output filename!\n")
            return False

        if (not self.commandline().hasOption("streamID") or not self.commandline().hasOption("parameter") or
                not self.commandline().hasOption("startTime") or not self.commandline().hasOption("endTime")):
            sys.stderr.write(
                "Please give me all query parameters (--streamID, --parameter, --startTime, --endTime)!\n")
            return False

        self._streamID = self.commandline().optionString("streamID")
        if re.search("[*?]", self._streamID):
            sys.stderr.write("Please do not use wildcards in the streamID!\n")
            return False

        self._parameter = self.commandline().optionString("parameter")
        # if not self._parameter in ([seiscomp.datamodel.EQCNameNames.name(i).replace(" ","_").upper() for i in xrange(seiscomp.datamodel.EQCNameQuantity)]):
        #sys.stderr.write("The given parameter name (%s) is unknown\n" % self._parameter)
        # return False

        self._start = self.commandline().optionString("startTime")
        self._end = self.commandline().optionString("endTime")

        return True

    def run(self):
        if not self.query():
            sys.stderr.write("No database connection!\n")
            return False

        xarc = seiscomp.io.XMLArchive()
        if not xarc.create(self._outfile, True, True):
            sys.stderr.write(
                "Could not create xml output file %s!\n" % self._outfile)
            return False

        xarc.setFormattedOutput(self.commandline().hasOption("formatted"))
        (net, sta, loc, cha) = self._streamID.split(".")
        it = self.query().getWaveformQuality(seiscomp.datamodel.WaveformStreamID(net, sta, loc, cha, ""),
                                             self._parameter,
                                             seiscomp.core.Time.FromString(
                                                 self._start, "%Y-%m-%d %H:%M:%S"),
                                             seiscomp.core.Time.FromString(self._end, "%Y-%m-%d %H:%M:%S"))
        while it.get():
            wfq = seiscomp.datamodel.WaveformQuality.Cast(it.get())
            xarc.writeObject(wfq)
            it.step()

        xarc.close()
        return True


app = WfqQuery(len(sys.argv), sys.argv)
sys.exit(app())
