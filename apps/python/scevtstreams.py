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
import traceback
import seiscomp.core, seiscomp.client, seiscomp.io, seiscomp.datamodel


def readXML(self):

    pickIDs = set()
    picks = []

    try:
        if self.inputFormat == "xml":
            ar = seiscomp.io.XMLArchive()
        elif self.inputFormat == "zxml":
            ar = seiscomp.io.XMLArchive()
            ar.setCompression(True)
        elif self.inputFormat == "binary":
            ar = seiscomp.io.VBinaryArchive()
        else:
            raise TypeError("unknown input format '" + inputFormat + "'")

        if ar.open(self.inputFile) == False:
            raise IOError(self.inputFile + ": unable to open")

        obj = ar.readObject()
        if obj is None:
            raise TypeError(self.inputFile + ": invalid format")

        ep = seiscomp.datamodel.EventParameters.Cast(obj)
        if ep is None:
            raise TypeError(self.inputFile + ": no eventparameters found")

        if ep.eventCount() <= 0:
            if ep.originCount() <= 0:
                raise TypeError(self.inputFile + \
                    ": no origin and no event in eventparameters found")
        else:
            if ep.eventCount() > 1:
                if not self.event:
                    sys.stderr.write(
                        "\nERROR: input contains more than 1 event. Considering only first event\n")

            for i in range(ep.eventCount()):
                ev = ep.event(i)
                if self.eventID:
                    if self.eventID != ev.publicID():
                        # sys.stderr.write("Ignoring event ID" + ev.publicID() + "\n")
                        continue

                # sys.stderr.write("Working on event ID" + ev.publicID() + "\n")
                for iorg in range(ev.originReferenceCount()):
                    org = seiscomp.datamodel.Origin.Find(
                        ev.originReference(iorg).originID())
                    if org is None:
                        continue
#           sys.stderr.write("Working on origin" + org.publicID() + "\n")

                    for iarrival in range(org.arrivalCount()):
                        pickID = org.arrival(iarrival).pickID()
                        pickIDs.add(pickID)

                for pickID in pickIDs:
                    pick = seiscomp.datamodel.Pick.Find(pickID)
                    picks.append(pick)

                break

    except Exception as exc:
        sys.stderr.write("ERROR: " + str(exc) + "\n")
        return False

    return picks


class EventStreams(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self.eventID = None
        self.inputFile = None
        self.margin = [300]

        self.allComponents = True
        self.allLocations = True

        self.streams = []

        self.caps = False
        self.fdsnws = False

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption("Input", "input,i",
                                           "input XML file name. Overwrites reading from database.")
        self.commandline().addStringOption("Input", "format,f", "input format to use (xml [default], zxml (zipped xml), binary). "
                                           "Only relevant with --input.")

        self.commandline().addGroup("Dump")
        self.commandline().addStringOption("Dump", "event,E", "event id")
        self.commandline().addStringOption("Dump", "margin,m",
                                           "time margin around the picked time window, default is 300. Added before the first and after the last pick, respectively. Use 2 comma-separted values (before,after) for asymmetric margins, e.g. -m 120,300.")
        self.commandline().addStringOption("Dump", "streams,S",
                                           "comma separated list of streams per station to add, e.g. BH,SH,HH")
        self.commandline().addOption("Dump", "all-streams",
                                     "dump all streams. If unused, just streams with picks are dumped.")
        self.commandline().addIntOption("Dump", "all-components,C",
                                        "all components or just the picked ones (0). Default is 1")
        self.commandline().addIntOption("Dump", "all-locations,L",
                                        "all locations or just the picked ones (0). Default is 1")
        self.commandline().addOption("Dump", "all-stations",
                                     "dump all stations from the same network. If unused, just stations with picks are dumped.")
        self.commandline().addOption("Dump", "all-networks", "dump all networks. If unused, just networks with picks are dumped. "
                                     "This option implies all-stations, all-locations, all-streams, all-components "
                                     "and will only provide the time window.")
        self.commandline().addOption("Dump", "resolve-wildcards,R",
                                     "if all components are used, use inventory to resolve stream components instead of using '?' (important when Arclink should be used)")
        self.commandline().addOption("Dump", "caps",
                                     "dump in capstool format (Common Acquisition Protocol Server by gempa GmbH)")
        self.commandline().addOption("Dump", "fdsnws",
                                     "dump in FDSN dataselect webservice POST format")
        return True

    def validateParameters(self):
        if self.commandline().hasOption("resolve-wildcards"):
            self.setLoadStationsEnabled(True)

        try:
            self.inputFile = self.commandline().optionString("input")
        except:
            pass

        if self.inputFile:
            self.setDatabaseEnabled(False, False)

        return True

    def init(self):
        try:
            if not seiscomp.client.Application.init(self):
                return False

            try:
                self.inputFormat = self.commandline().optionString("format")
            except:
                self.inputFormat = "xml"

            try:
                self.eventID = self.commandline().optionString("event")
            except:
                sys.stderr.write("Error: An eventID is mandatory\n")
                return False

            try:
                self.margin = self.commandline().optionString("margin").split(",")
            except:
                pass

            try:
                self.streams = self.commandline().optionString("streams").split(",")
            except:
                pass

            try:
                self.allComponents = self.commandline().optionInt("all-components") != 0
            except:
                pass

            try:
                self.allLocations = self.commandline().optionInt("all-locations") != 0
            except:
                pass

            self.allStreams = self.commandline().hasOption("all-streams")
            self.allStations = self.commandline().hasOption("all-stations")
            self.allNetworks = self.commandline().hasOption("all-networks")
            self.caps = self.commandline().hasOption("caps")
            self.fdsnws = self.commandline().hasOption("fdsnws")

            return True
        except:
            cla, exc, trbk = sys.exc_info()
            sys.stderr.write("%s\n" % cla.__name__)
            sys.stderr.write("%s\n" % exc.__dict__["args"])
            sys.stderr.write("%s\n" % traceback.format_tb(trbk, 5))

    def run(self):
        try:
            picks = []
            minTime = None
            maxTime = None

            self.marginBefore = int(self.margin[0])
            self.marginAfter = int(self.margin[-1])

            resolveWildcards = self.commandline().hasOption("resolve-wildcards")

            if self.inputFile == None:
                # sys.stderr.write("Reading from database\n")
                for obj in self.query().getEventPicks(self.eventID):
                    pick = seiscomp.datamodel.Pick.Cast(obj)
                    if pick is None:
                        continue
                    picks.append(pick)
            else:
                # sys.stderr.write("Reading from XML input. Ignoring events in database\n")
                picks = readXML(self)

            if not picks:
                sys.stderr.write(
                    "Could not find picks for event " + self.eventID)
                if self.inputFile == None:
                    sys.stderr.write(" in database\n")
                else:
                    sys.stderr.write(" in input file " + self.inputFile + "\n")

            for pick in picks:
                if minTime is None:
                    minTime = pick.time().value()
                elif minTime > pick.time().value():
                    minTime = pick.time().value()

                if maxTime is None:
                    maxTime = pick.time().value()
                elif maxTime < pick.time().value():
                    maxTime = pick.time().value()

            if minTime:
                minTime = minTime - seiscomp.core.TimeSpan(self.marginBefore)
            if maxTime:
                maxTime = maxTime + seiscomp.core.TimeSpan(self.marginAfter)

            inv = seiscomp.client.Inventory.Instance().inventory()

            lines = set()
            for pick in picks:
                net = pick.waveformID().networkCode()
                station = pick.waveformID().stationCode()
                loc = pick.waveformID().locationCode()
                streams = [pick.waveformID().channelCode()]
                rawStream = streams[0][:2]

                if self.allComponents == True:
                    if resolveWildcards:
                        iloc = seiscomp.datamodel.getSensorLocation(inv, pick)
                        if iloc:
                            tc = seiscomp.datamodel.ThreeComponents()
                            seiscomp.datamodel.getThreeComponents(
                                tc, iloc, rawStream, pick.time().value())
                            streams = []
                            if tc.vertical():
                                streams.append(tc.vertical().code())
                            if tc.firstHorizontal():
                                streams.append(tc.firstHorizontal().code())
                            if tc.secondHorizontal():
                                streams.append(tc.secondHorizontal().code())
                    else:
                        streams = [rawStream + "?"]

                if self.allLocations == True:
                    loc = "*"

                if self.allStations:
                    station = "*"

                if self.allNetworks:
                    net = "*"
                    station = "*"
                    loc = "*"

                for s in streams:
                    if self.allStreams or self.allNetworks:
                        s = "*"

                    if self.caps:
                        line = minTime.toString("%Y,%m,%d,%H,%M,%S") + " " + maxTime.toString("%Y,%m,%d,%H,%M,%S") + " " \
                            + net + " " + station \
                            + " " + loc + " " + s
                    elif self.fdsnws:
                        line = net + " " + station + " " + loc + " " + s + " " + minTime.iso() + " " + maxTime.iso()
                    else:
                        line = minTime.toString("%F %T") + ";" + maxTime.toString("%F %T") + ";" \
                            + net + "." + station \
                            + "." + loc + "." + s

                    lines.add(line)

                for s in self.streams:
                    if s != rawStream:

                        if self.allStreams or self.allNetworks:
                            s = "*"

                        if self.caps:
                            line = minTime.toString("%Y,%m,%d,%H,%M,%S") + " " + maxTime.toString("%Y,%m,%d,%H,%M,%S") + " " \
                                + net + " " + station + " " + \
                                loc + " " + s + streams[0][2]
                        elif self.fdsnws:
                            line = net + " " + station + " " + loc + " " + s + " " + minTime.iso() + " " + maxTime.iso()
                        else:
                            line = minTime.toString("%F %T") + ";" + maxTime.toString("%F %T") + ";" \
                                + net + "." + station + "." + \
                                loc + "." + s + streams[0][2]

                        lines.add(line)

            for line in sorted(lines):
                sys.stdout.write(line+"\n")

            return True
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)


app = EventStreams(len(sys.argv), sys.argv)
sys.exit(app())
