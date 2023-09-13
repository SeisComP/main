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

from __future__ import absolute_import, division, print_function

import sys
import re

from seiscomp import client, core, datamodel, io


def readStreamList(listFile):
    """
    Read list of streams from file

    Parameters
    ----------
    file : file
        Input list file, one line per stream
        format: NET.STA.LOC.CHA

    Returns
    -------
    list
        streams.

    """
    streams = []

    try:
        if listFile == "-":
            f = sys.stdin
            listFile = "stdin"
        else:
            f = open(listFile, "r", encoding="utf8")
    except Exception:
        print(f"error: unable to open '{listFile}'", file=sys.stderr)
        return []

    lineNumber = -1
    for line in f:
        lineNumber = lineNumber + 1
        line = line.strip()
        # ignore comments
        if len(line) > 0 and line[0] == "#":
            continue

        if len(line) == 0:
            continue

        if len(line.split(".")) != 4:
            f.close()
            print(
                f"error: {listFile} in line {lineNumber} has invalid line format, "
                "expecting NET.STA.LOC.CHA - 1 line per stream",
                file=sys.stderr,
            )
            return []
        streams.append(line)

    f.close()

    if len(streams) == 0:
        return []

    return streams


class EventStreams(client.Application):
    def __init__(self, argc, argv):
        client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self.eventID = None
        self.inputFile = None
        self.inputFormat = "xml"
        self.margin = [300]

        self.allNetworks = True
        self.allStations = True
        self.allLocations = True
        self.allStreams = True
        self.allComponents = True

        # filter
        self.network = None
        self.station = None

        self.streams = []
        self.streamFilter = None

        # output format
        self.caps = False
        self.fdsnws = False

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Input XML file name. Reads event from the XML file instead of database. "
            "Use '-' to read from stdin.",
        )
        self.commandline().addStringOption(
            "Input",
            "format,f",
            "Input format to use (xml [default], zxml (zipped xml), binary). "
            "Only relevant with --input.",
        )

        self.commandline().addGroup("Dump")
        self.commandline().addStringOption(
            "Dump", "event,E", "The ID of the event to consider."
        )
        self.commandline().addStringOption(
            "Dump",
            "net-sta",
            "Filter read picks by network code or network and station code. Format: "
            "NET or NET.STA.",
        )
        self.commandline().addStringOption(
            "Dump",
            "nslc",
            "Stream list file to be used for filtering read picks by stream code. "
            "'--net-sta' will be ignored. One line per stream, line format: "
            "NET.STA.LOC.CHA.",
        )

        self.commandline().addGroup("Output")
        self.commandline().addStringOption(
            "Output",
            "margin,m",
            "Time margin around the picked time window, default is 300. Added "
            "before the first and after the last pick, respectively. Use 2 "
            "comma-separted values (before,after) for asymmetric margins, e.g. "
            "-m 120,300.",
        )
        self.commandline().addStringOption(
            "Output",
            "streams,S",
            "Comma-separated list of streams per station to add, e.g. BH,SH,HH.",
        )
        self.commandline().addOption(
            "Output",
            "all-streams",
            "Dump all streams. If unused, just streams with picks are dumped.",
        )
        self.commandline().addIntOption(
            "Output",
            "all-components,C",
            "All components or just the picked ones (0). Default is 1",
        )
        self.commandline().addIntOption(
            "Output",
            "all-locations,L",
            "All locations or just the picked ones (0). Default is 1",
        )
        self.commandline().addOption(
            "Output",
            "all-stations",
            "Dump all stations from the same network. If unused, just stations "
            "with picks are dumped.",
        )
        self.commandline().addOption(
            "Output",
            "all-networks",
            "Dump all networks. If unused, just networks with picks are dumped."
            " This option implies --all-stations, --all-locations, --all-streams, "
            "--all-components and will only provide the time window.",
        )
        self.commandline().addOption(
            "Output",
            "resolve-wildcards,R",
            "If all components are used, use inventory to resolve stream "
            "components instead of using '?' (important when Arclink should be "
            "used).",
        )
        self.commandline().addOption(
            "Output",
            "caps",
            "Output in capstool format (Common Acquisition Protocol Server by "
            "gempa GmbH).",
        )
        self.commandline().addOption(
            "Output", "fdsnws", "Output in FDSN dataselect webservice POST format."
        )
        return True

    def validateParameters(self):
        if not client.Application.validateParameters(self):
            return False

        if self.commandline().hasOption("resolve-wildcards"):
            self.setLoadStationsEnabled(True)

        try:
            self.inputFile = self.commandline().optionString("input")
            self.setDatabaseEnabled(False, False)
        except BaseException:
            pass

        return True

    def init(self):
        if not client.Application.init(self):
            return False

        try:
            self.inputFormat = self.commandline().optionString("format")
        except BaseException:
            pass

        try:
            self.eventID = self.commandline().optionString("event")
        except BaseException:
            if not self.inputFile:
                raise ValueError(
                    "An eventID is mandatory if no input file is " "specified"
                )

        try:
            self.margin = self.commandline().optionString("margin").split(",")
        except BaseException:
            pass

        try:
            self.streams = self.commandline().optionString("streams").split(",")
        except BaseException:
            pass

        try:
            self.allComponents = self.commandline().optionInt("all-components") != 0
        except BaseException:
            pass

        try:
            self.allLocations = self.commandline().optionInt("all-locations") != 0
        except BaseException:
            pass

        self.allStreams = self.commandline().hasOption("all-streams")
        self.allStations = self.commandline().hasOption("all-stations")
        self.allNetworks = self.commandline().hasOption("all-networks")

        try:
            networkStation = self.commandline().optionString("net-sta")
        except RuntimeError:
            networkStation = None

        try:
            nslcFile = self.commandline().optionString("nslc")
        except RuntimeError:
            nslcFile = None

        if nslcFile:
            networkStation = None
            self.streamFilter = readStreamList(nslcFile)

        if networkStation:
            try:
                self.network = networkStation.split(".")[0]
            except IndexError:
                print(
                    f"Error in network code '{networkStation}': Use '--net-sta' with "
                    "format NET or NET.STA",
                    file=sys.stderr,
                )
                return False

            try:
                self.station = networkStation.split(".")[1]
            except IndexError:
                pass

        self.caps = self.commandline().hasOption("caps")
        self.fdsnws = self.commandline().hasOption("fdsnws")

        return True

    def printUsage(self):
        print(
            """Usage:
  scevtstreams [options]

Extract stream information and time windows from an event"""
        )

        client.Application.printUsage(self)

        print(
            """Examples:
Get the time windows for an event in the database:
  scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp

Create lists compatible with fdsnws:
  scevtstreams -E gfz2012abcd -i event.xml -m 120,500 --fdsnws
"""
        )

    def run(self):
        resolveWildcards = self.commandline().hasOption("resolve-wildcards")

        picks = []

        # read picks from input file
        if self.inputFile:
            picks = self.readXML()
            if not picks:
                raise ValueError("Could not find picks in input file")

        # read picks from database
        else:
            for obj in self.query().getEventPicks(self.eventID):
                pick = datamodel.Pick.Cast(obj)
                if pick is None:
                    continue
                picks.append(pick)

            if not picks:
                raise ValueError(
                    f"Could not find picks for event {self.eventID} in database"
                )

        # filter picks
        if self.streamFilter:
            # # filter channel by --nslc option
            channels = self.streamFilter
            channelsRe = []
            for channel in channels:
                channel = re.sub(r"\.", r"\.", channel)  # . becomes \.
                channel = re.sub(r"\?", ".", channel)  # ? becomes .
                channel = re.sub(r"\*", ".*", channel)  # * becomes.*
                channel = re.compile(channel)
                channelsRe.append(channel)

        if self.streamFilter or self.network:
            pickFiltered = []
            for pick in picks:
                net = pick.waveformID().networkCode()
                sta = pick.waveformID().stationCode()
                loc = pick.waveformID().locationCode()
                cha = pick.waveformID().channelCode()

                filtered = False
                if self.streamFilter:
                    stream = f"{net}.{sta}.{loc}.{cha}"
                    for chaRe in channelsRe:
                        if chaRe.match(stream):
                            filtered = True
                            continue

                elif self.network:
                    if net != self.network:
                        continue
                    if self.station and sta != self.station:
                        continue
                    filtered = True

                if filtered:
                    pickFiltered.append(pick)
                else:
                    print(
                        f"Ignoring channel {stream}: not considered by configuration",
                        file=sys.stderr,
                    )

            picks = pickFiltered

        if not picks:
            raise ValueError("Info: All picks are filtered out")

        # calculate minimum and maximum pick time
        minTime = None
        maxTime = None
        for pick in picks:
            if minTime is None or minTime > pick.time().value():
                minTime = pick.time().value()

            if maxTime is None or maxTime < pick.time().value():
                maxTime = pick.time().value()

        # add time margin(s), no need for None check since pick time is
        # mandatory and at least on pick exists
        minTime = minTime - core.TimeSpan(float(self.margin[0]))
        maxTime = maxTime + core.TimeSpan(float(self.margin[-1]))

        # convert times to string dependend on requested output format
        if self.caps:
            timeFMT = "%Y,%m,%d,%H,%M,%S"
        elif self.fdsnws:
            timeFMT = "%FT%T"
        else:
            timeFMT = "%F %T"
        minTime = minTime.toString(timeFMT)
        maxTime = maxTime.toString(timeFMT)

        inv = client.Inventory.Instance().inventory()

        lines = set()
        for pick in picks:
            net = pick.waveformID().networkCode()
            station = pick.waveformID().stationCode()
            loc = pick.waveformID().locationCode()
            streams = [pick.waveformID().channelCode()]
            rawStream = streams[0][:2]

            if self.allComponents:
                if resolveWildcards:
                    iloc = datamodel.getSensorLocation(inv, pick)
                    if iloc:
                        tc = datamodel.ThreeComponents()
                        datamodel.getThreeComponents(
                            tc, iloc, rawStream, pick.time().value()
                        )
                        streams = []
                        if tc.vertical():
                            streams.append(tc.vertical().code())
                        if tc.firstHorizontal():
                            streams.append(tc.firstHorizontal().code())
                        if tc.secondHorizontal():
                            streams.append(tc.secondHorizontal().code())
                else:
                    streams = [rawStream + "?"]

            if self.allLocations:
                loc = "*"

            if self.allStations:
                station = "*"

            if self.allNetworks:
                net = "*"
                station = "*"
                loc = "*"

            # FDSNWS requires empty location to be encoded by 2 dashes
            if not loc and self.fdsnws:
                loc = "--"

            # line format
            if self.caps:
                lineFMT = "{0} {1} {2} {3} {4} {5}"
            elif self.fdsnws:
                lineFMT = "{2} {3} {4} {5} {0} {1}"
            else:
                lineFMT = "{0};{1};{2}.{3}.{4}.{5}"

            for s in streams:
                if self.allStreams or self.allNetworks:
                    s = "*"

                lines.add(lineFMT.format(minTime, maxTime, net, station, loc, s))

            for s in self.streams:
                if s == rawStream:
                    continue

                if self.allStreams or self.allNetworks:
                    s = "*"

                lines.add(
                    lineFMT.format(
                        minTime, maxTime, net, station, loc, s + streams[0][2]
                    )
                )

        for line in sorted(lines):
            print(line, file=sys.stdout)

        return True

    def readXML(self):
        if self.inputFormat == "xml":
            ar = io.XMLArchive()
        elif self.inputFormat == "zxml":
            ar = io.XMLArchive()
            ar.setCompression(True)
        elif self.inputFormat == "binary":
            ar = io.VBinaryArchive()
        else:
            raise TypeError(f"unknown input format '{self.inputFormat}'")

        if not ar.open(self.inputFile):
            raise IOError("unable to open input file")

        obj = ar.readObject()
        if obj is None:
            raise TypeError("invalid input file format")

        ep = datamodel.EventParameters.Cast(obj)
        if ep is None:
            raise ValueError("no event parameters found in input file")

        # we require at least one origin which references to picks via arrivals
        if ep.originCount() == 0:
            raise ValueError("no origin found in input file")

        originIDs = []

        # search for a specific event id
        if self.eventID:
            ev = datamodel.Event.Find(self.eventID)
            if ev:
                originIDs = [
                    ev.originReference(i).originID()
                    for i in range(ev.originReferenceCount())
                ]
            else:
                raise ValueError(f"Event ID {self.eventID} not found in input file")

        # use first event/origin if no id was specified
        else:
            # no event, use first available origin
            if ep.eventCount() == 0:
                if ep.originCount() > 1:
                    print(
                        "WARNING: Input file contains no event but more than "
                        "1 origin. Considering only first origin",
                        file=sys.stderr,
                    )
                originIDs.append(ep.origin(0).publicID())

            # use origin references of first available event
            else:
                if ep.eventCount() > 1:
                    print(
                        "WARNING: Input file contains more than 1 event. "
                        "Considering only first event",
                        file=sys.stderr,
                    )
                ev = ep.event(0)
                originIDs = [
                    ev.originReference(i).originID()
                    for i in range(ev.originReferenceCount())
                ]

        # collect pickIDs
        pickIDs = set()
        for oID in originIDs:
            o = datamodel.Origin.Find(oID)
            if o is None:
                continue

            for i in range(o.arrivalCount()):
                pickIDs.add(o.arrival(i).pickID())

        # lookup picks
        picks = []
        for pickID in pickIDs:
            pick = datamodel.Pick.Find(pickID)
            if pick:
                picks.append(pick)

        return picks


if __name__ == "__main__":
    try:
        app = EventStreams(len(sys.argv), sys.argv)
        sys.exit(app())
    except (ValueError, TypeError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
    sys.exit(1)
