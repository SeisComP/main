#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) 2021 by gempa GmbH                                         #
# All rights reserved.                                                     #
#                                                                          #
# GNU Affero General Public License Usage                                  #
# This file may be used under the terms of the GNU Affero                  #
# Public License version 3.0 as published by the Free Software Foundation  #
# and appearing in the file LICENSE included in the packaging of this      #
# file. Please review the following information to ensure the GNU Affero   #
# Public License version 3.0 requirements will be met:                     #
# https://www.gnu.org/licenses/agpl-3.0.html.                              #
#                                                                          #
#  adopted from scqcquery                                                  #
#  Author: Dirk Roessler, gempa GmbH                                       #
#  Email: roessler@gempa.de                                                #
############################################################################

from __future__ import absolute_import, division, print_function

import sys
import re
import seiscomp.core
import seiscomp.client
import seiscomp.io
import seiscomp.datamodel

qcParamsDefault = "latency,delay,timing,offset,rms,availability,"\
                  "'gaps count','gaps interval','gaps length',"\
                  "'overlaps count','overlaps interval','overlaps length',"\
                  "'spikes count','spikes interval','spikes amplitude'"

def getStreamsFromInventory(self):
    streams = []
    try:
        dbr = seiscomp.datamodel.DatabaseReader(self.database())
        inv = seiscomp.datamodel.Inventory()
        dbr.loadNetworks(inv)

        streamList = set()
        for inet in range(inv.networkCount()):
            network = inv.network(inet)
            dbr.load(network)
            for ista in range(network.stationCount()):
                station = network.station(ista)
                try:
                    start = station.start()
                except Exception:
                    continue
                try:
                    end = station.end()
                    if not start <= self._end <= end and end >= self._start:
                        continue
                except Exception:
                    pass

                for iloc in range(station.sensorLocationCount()):
                    location = station.sensorLocation(iloc)
                    for istr in range(location.streamCount()):
                        stream = location.stream(istr)
                        streamID = network.code() \
                                   + "." + station.code() \
                                   + "." + location.code() \
                                   + "." + stream.code()
                        streamList.add(streamID)

        streams = list(streamList)
        print(streams)
        return streams

    except Exception:
        return False



class WfqQuery(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setLoggingToStdErr(True)
        self.setDaemonEnabled(False)

    def createCommandLineDescription(self):
        self.commandline().addGroup("Output")
        self.commandline().addStringOption("Output", "output,o", "output file name for "
                                           "XML. Writes to stdout if not given.")
        self.commandline().addOption("Output", "formatted,f", "write formatted XML")

        self.commandline().addGroup("Query")
        self.commandline().addOption("Query", "streams-from-inventory",
                                     "read streams from inventory. Superseeded by streamID.")
        self.commandline().addStringOption("Query", "streamID,i",
                                           "waveform stream ID: net.sta.loc.cha - "
                                           "<networkCode>.<stationCode>."
                                           "<sensorLocationCode>.<channelCode>"
                                           " Provide a single ID or a comma-separated list. "
                                           "Overrides '--streams-from-inventory'")
        self.commandline().addStringOption("Query", "parameter,p",
                                           "QC parameter to output: (e.g. delay, rms, "
                                           "'gaps count' ...). Provide a single parameter "
                                           "or a comma-separated list. Defaults "
                                           "apply if paramaeter is not given.")
        self.commandline().addStringOption(
            "Query", "begin,b", "start time: 'YYYY-MM-DD hh:mm:ss'")
        self.commandline().addStringOption(
            "Query", "end,e", "end time: 'YYYY-MM-DD hh:mm:ss'")

        return True

    def printUsage(self):

        print("\nscquery_qc - query a database for waveform quality control parameters\n",
              file=sys.stderr)
        print("Usage: scquery_qc [options]", file=sys.stderr)
        seiscomp.client.Application.printUsage(self)

        print("Default QC parameters: {}\n".format(qcParamsDefault),
              file=sys.stderr)
        print("Examples:", file=sys.stderr)
        print("\n  Query rms and delay values for streams 'AU.AS18..SHZ' and "
              "'AU.AS19..SHZ' from '2021-11-20 00:00:00' until current", file=sys.stderr)
        print("    scquery_qc -d localhost -b '2021-11-20 00:00:00' "
              "-p rms,delay -i AU.AS18..SHZ,AU.AS19..SHZ\n", file=sys.stderr)

    def validateParameters(self):
        try:
            self._streams = self.commandline().optionString("streamID").split(",")
        except RuntimeError:
            self._streams = False

        try:
            self._fromInventory =  self.commandline().hasOption("streams-from-inventory")
        except RuntimeError:
            self._fromInventory = False

        if not self._streams and not self._fromInventory:
            print("Provide streamID(s): --streamID or --streams-from-inventory\n",
                  file=sys.stderr)
            return False

        try:
            self._outfile = self.commandline().optionString("output")
        except RuntimeError:
            self._outfile = '-'
            print("No output file name given: Sending to stdout\n", file=sys.stderr)

        try:
            self._start = self.commandline().optionString("begin")
        except RuntimeError:
            print("Must provide start time: --begin", file=sys.stderr)
            return False

        try:
            self._end = self.commandline().optionString("end")
        except RuntimeError:
            self._end = str(seiscomp.core.Time.GMT())
            print("No end time given, considering 'now': {}".format(self._end),
                  file=sys.stderr)

        try:
            self._parameter = self.commandline().optionString("parameter")
        except RuntimeError:
            self._parameter = qcParamsDefault
            print("No QC parameter given, using default\n", file=sys.stderr)

        try:
            self._formatted = self.commandline().hasOption("formatted")
        except RuntimeError:
            self._formatted = False

        return True

    def run(self):
        if not self.query():
            print("No database connection!\n", file=sys.stderr)
            return False

        streams = self._streams
        if not streams and self._fromInventory:
            try:
                streams = getStreamsFromInventory(self)
            except RuntimeError:
                print("No streams read from database!\n", file=sys.stderr)
                return False

        if not streams:
            print("Empty stream list")
            return False

        for stream in streams:
            if re.search("[*?]", stream):
                print("Wildcards in streamID are not supported: {}\n".format(stream),
                      file=sys.stderr)
                return False

        print("Request:", file=sys.stderr)
        print("  begin time:  {}".format(str(self._start)), file=sys.stderr)
        print("  end time:    {}".format(str(self._end)), file=sys.stderr)
        print("  streams:     {}".format(str(streams)), file=sys.stderr)
        print("  parameters:  {}".format(str(self._parameter)), file=sys.stderr)
        print("Output:", file=sys.stderr)
        print("  file:        {}".format(self._outfile), file=sys.stderr)
        print("  formatted:   {}".format(self._formatted), file=sys.stderr)

        # create archive
        xarc = seiscomp.io.XMLArchive()
        if not xarc.create(self._outfile, True, True):
            print("Unable to write XML to {}!\n".format(self._outfile), file=sys.stderr)
            return False
        xarc.setFormattedOutput(self._formatted)

        # write parameters
        for parameter in self._parameter.split(","):
            for stream in streams:
                (net, sta, loc, cha) = stream.split(".")
                it = self.query().getWaveformQuality(seiscomp.datamodel.WaveformStreamID(net, sta, loc, cha, ""),
                                                     parameter,
                                                     seiscomp.core.Time.FromString(
                                                         self._start, "%Y-%m-%d %H:%M:%S"),
                                                     seiscomp.core.Time.FromString(self._end, "%Y-%m-%d %H:%M:%S"))

                while it.get():
                    try:
                        wfq = seiscomp.datamodel.WaveformQuality.Cast(it.get())
                        xarc.writeObject(wfq)
                    except Exception:
                        pass
                    it.step()

        xarc.close()
        return True


app = WfqQuery(len(sys.argv), sys.argv)
sys.exit(app())
