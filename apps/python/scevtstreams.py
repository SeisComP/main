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

import re
import sys

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

        self.eventIDs = None
        self.inputFile = None
        self.inputFormat = "xml"
        self.margin = [300]

        self.allNetworks = True
        self.allStations = True
        self.allLocations = True
        self.allStreams = True
        self.allComponents = True
        self.usedArrivals = False

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
            "Input XML file name. Reads EventParameters from the XML file instead of "
            "database. Use '-' to read from stdin.",
        )
        self.commandline().addStringOption(
            "Input",
            "format,f",
            "Input format to use (xml [default], zxml (zipped xml), binary). "
            "Only relevant with --input.",
        )

        self.commandline().addGroup("Dump")
        self.commandline().addStringsOption(
            "Dump",
            "event,E",
            "The ID of the event to consider. "
            "Repeat the option to consider multiple events. "
            "Use '-' to  read the IDs as individual lines from stdin.",
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
            "used-arrivals",
            "Consider only arrivals which have been used for locating. "
            "Ignore all unused arrivals.",
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
            self.eventIDs = self.commandline().optionStrings("event")
            # Check if eventIDs are read from stdin
            if "-" in self.eventIDs:
                # Append if -E - is additionally given with -E evID
                self.eventIDs = [i for i in self.eventIDs if i != "-"]
                for line in sys.stdin:
                    evID = line.strip()
                    if evID == "":
                        continue
                    self.eventIDs.append(evID)

        except BaseException as exc:
            if not self.inputFile:
                raise ValueError(
                    "An eventID is mandatory if no input file is specified"
                ) from exc

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
        self.usedArrivals = self.commandline().hasOption("used-arrivals")

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

Extract stream information and time windows from picks of an event or solitary picks."""
        )

        client.Application.printUsage(self)

        print(
            """Examples:
Get the time windows for an event in the database:
  scevtstreams -E gfz2012abcd -d mysql://sysop:sysop@localhost/seiscomp

Get the time windows for all picks given in an XML file without origins and events:
  scevtstreams -i picks.xml -m 120,500
"""
        )

    def run(self):
        resolveWildcards = self.commandline().hasOption("resolve-wildcards")

        # read picks from input file
        if self.inputFile:
            try:
                evPicks, objIDs = self.readXML()
            except IOError as e:
                print(f"Error: {e}", file=sys.stderr)
                return False

            if not evPicks:
                raise ValueError("Could not find picks in input file")

        # read picks from database
        else:
            evPicks = self.readDB()
            objIDs = self.eventIDs

        # setup channel filter by --nslc option
        channelsRe = []
        if self.streamFilter and evPicks:
            channels = self.streamFilter
            for channel in channels:
                channel = re.sub(r"\.", r"\.", channel)  # . becomes \.
                channel = re.sub(r"\?", ".", channel)  # ? becomes .
                channel = re.sub(r"\*", ".*", channel)  # * becomes.*
                channel = re.compile(channel)
                channelsRe.append(channel)

        # go through picks per event
        linesDict = {}
        for objID, picks in zip(objIDs, evPicks):
            # filter picks
            if self.streamFilter or self.network:
                picks = self.filterPicks(picks, channelsRe)

            if not picks:
                print(
                    f"Info: All picks are filtered out for {objID}",
                    file=sys.stderr,
                )
                continue

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

            inv = client.Inventory.Instance().inventory()
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
                        streams = [f"{rawStream}?"]

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

                for s in streams:
                    if self.allStreams or self.allNetworks:
                        s = "*"

                    # gathering streams
                    nslc = f"{net}.{station}.{loc}.{s}"
                    if nslc not in linesDict:
                        linesDict[nslc] = {}
                        num = 0
                    else:
                        num = max(linesDict[nslc].keys()) + 1
                    linesDict[nslc][num] = {"minTime": minTime, "maxTime": maxTime}

                for s in self.streams:
                    if s == rawStream:
                        continue

                    if self.allStreams or self.allNetworks:
                        s = "*"
                    else:
                        s = f"{s}{streams[0][2]}"

                    # gathering streams
                    nslc = f"{net}.{station}.{loc}.{s}"
                    if nslc not in linesDict:
                        linesDict[nslc] = {}
                        num = 0
                    else:
                        num = max(linesDict[nslc].keys()) + 1
                    linesDict[nslc][num] = {"minTime": minTime, "maxTime": maxTime}

        # try to merge lines with overlapping time windows by nslc
        mergedLinesDict = {}
        for nslc, l in linesDict.items():
            if max(l.keys()) == 0:
                mergedLinesDict[nslc] = linesDict[nslc]
                continue

            mergedLinesDict[nslc] = self.mergeStreamTimeWindows(linesDict[nslc])

        # line format
        # convert times to string dependent on requested output format
        if self.caps:
            lineFMT = "{0} {1} {2} {3} {4} {5}"
            timeFMT = "%Y,%m,%d,%H,%M,%S"
        elif self.fdsnws:
            lineFMT = "{2} {3} {4} {5} {0} {1}"
            timeFMT = "%FT%T"
        else:
            lineFMT = "{0};{1};{2}.{3}.{4}.{5}"
            timeFMT = "%F %T"

        # outputs formated strings
        for nslc, val in sorted(mergedLinesDict.items()):
            n, s, l, c = nslc.rsplit(".")
            for num, times in val.items():
                minTime = times["minTime"].toString(timeFMT)
                maxTime = times["maxTime"].toString(timeFMT)
                line = lineFMT.format(minTime, maxTime, n, s, l, c)
                print(line, file=sys.stdout)
        return True

    def filterPicks(self, picks, channelsRe):
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

        return pickFiltered

    def mergeStreamTimeWindows(self, inDict):
        mergedDict = {}
        tmpDict = inDict

        # loop, until all overlapping time windows are matched
        continueFlag = True
        while continueFlag:
            mergeFlag = False
            blacklist = []

            # if dict has only a length of 1 left, leave
            if len(tmpDict.keys()) == 1:
                mergedDict = tmpDict
                break
            for num1, timeDict1 in tmpDict.items():
                foundFlag = False
                maxTime1 = timeDict1["maxTime"]
                minTime1 = timeDict1["minTime"]
                if num1 in blacklist:
                    continue

                for num2, timeDict2 in tmpDict.items():
                    maxTime2 = timeDict2["maxTime"]
                    minTime2 = timeDict2["minTime"]

                    if num2 <= num1:
                        continue

                    # Condition: any time stamp must be within the time window
                    cond1 = maxTime2 >= maxTime1 >= minTime2
                    cond2 = minTime2 <= minTime1 <= maxTime2
                    cond3 = maxTime1 >= maxTime2 >= minTime1
                    cond4 = minTime1 <= minTime2 <= maxTime1
                    if cond1 or cond2 or cond3 or cond4:
                        newMinTime = min(minTime1, minTime2)
                        newMaxTime = max(maxTime1, maxTime2)
                        mergedDict[num1] = {
                            "minTime": newMinTime,
                            "maxTime": newMaxTime,
                        }
                        blacklist.append(num2)
                        mergeFlag = True
                        foundFlag = True
                        break

                if not foundFlag:
                    mergedDict[num1] = tmpDict[num1]

            if mergeFlag:
                tmpDict = mergedDict
                mergedDict = {}
            else:
                continueFlag = False
        return mergedDict

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
            # pick may be provided as base object, only one can be read
            pick = datamodel.Pick.Cast(obj)
            if pick is None:
                raise ValueError(
                    "Neither event parameters nor pick found in input file"
                )
            print(
                "WARNING: Input file contains just one toplevel Pick object.",
                file=sys.stderr,
            )
            return [[pick]], [[pick.publicID()]]

        # we require at least one origin which references to picks via arrivals
        if ep.originCount() == 0 and ep.pickCount() == 0:
            raise ValueError("No origin found in input file")

        # Origin lists, one sub-list per event
        evOriginIDs = []
        objIDs = []
        # search for a specific event ids, collect originIDs
        if self.eventIDs:
            for eventID in self.eventIDs:
                ev = datamodel.Event.Find(eventID)
                if ev:
                    evOriginIDs.append(
                        [
                            ev.originReference(i).originID()
                            for i in range(ev.originReferenceCount())
                        ]
                    )
                else:
                    raise ValueError(f"Event ID '{eventID}' not found in input file")

            objIDs = self.eventIDs

        # use all events/origins if no id was specified
        else:
            # no event, use all available origins
            if ep.eventCount() == 0 and ep.originCount() > 0:
                print(
                    "WARNING: Input file contains no event but contains "
                    f"{ep.originCount()} origin(s). "
                    "Considering each origin separately.",
                    file=sys.stderr,
                )
                for ocnt in range(ep.originCount()):
                    evOriginIDs.append([ep.origin(ocnt).publicID()])

                objIDs = evOriginIDs

            # use all origins of all available events
            elif ep.eventCount() > 0 and ep.originCount() > 0:
                print(
                    f"WARNING: Input file contains {ep.eventCount()} event(s). "
                    "Considering all events",
                    file=sys.stderr,
                )

                for ecnt in range(ep.eventCount()):
                    ev = ep.event(ecnt)
                    evOriginIDs.append(
                        [
                            ev.originReference(i).originID()
                            for i in range(ev.originReferenceCount())
                        ]
                    )
                    objIDs.append(ev.publicID())
            else:
                print("Found no origins, trying to continue with picks only.")
                #  try reading picks only
                picks = []
                for i in range(ep.pickCount()):
                    pick = datamodel.Pick.Find(ep.pick(i).publicID())
                    if pick:
                        picks.append(pick)
                        objIDs.append(pick.publicID())
                return [picks], objIDs

        if evOriginIDs:
            print(
                "Considering all arrivals from "
                f"{sum(len(sublist) for sublist in evOriginIDs)} origin(s).",
                file=sys.stderr,
            )

        # select picks
        evPicks = []
        for oIDs in evOriginIDs:
            pickIDs = set()
            for oID in oIDs:
                # collect pickIDs from origins
                o = datamodel.Origin.Find(oID)
                if o is None:
                    continue

                for i in range(o.arrivalCount()):
                    # filter if used
                    if self.usedArrivals and not (
                        o.arrival(i).timeUsed()
                        or o.arrival(i).horizontalSlownessUsed()
                        or o.arrival(i).backazimuthUsed()
                    ):
                        continue

                    pickIDs.add(o.arrival(i).pickID())

            if len(pickIDs) == 0:
                print("Found no picks.", file=sys.stderr)

            # lookup picks
            picks = []
            for pickID in pickIDs:
                pick = datamodel.Pick.Find(pickID)
                if pick:
                    picks.append(pick)

            evPicks.append(picks)

        return evPicks, objIDs

    def readDB(self):
        evPicks = []
        for eventID in self.eventIDs:
            picks = []
            # filter by used arrival
            pickIDs = set()
            if self.usedArrivals:
                # collect all origins
                oris = []
                for obj in self.query().getOrigins(eventID):
                    oris.append(datamodel.Origin.Cast(obj))

                # check each arrival if used and collect pickID
                for o in oris:
                    for i in range(self.query().loadArrivals(o)):
                        # filter if used
                        if not (
                            o.arrival(i).timeUsed()
                            or o.arrival(i).horizontalSlownessUsed()
                            or o.arrival(i).backazimuthUsed()
                        ):
                            continue
                        pickIDs.add(o.arrival(i).pickID())

            for obj in self.query().getEventPicks(eventID):
                pick = datamodel.Pick.Cast(obj)
                if pick is None:
                    continue

                if self.usedArrivals and pick.publicID() not in pickIDs:
                    continue

                picks.append(pick)

            if not picks:
                raise ValueError(
                    f"Could not find picks for event {eventID} in database"
                )

            evPicks.append(picks)

        return evPicks


if __name__ == "__main__":
    try:
        app = EventStreams(len(sys.argv), sys.argv)
        sys.exit(app())
    except (ValueError, TypeError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
    sys.exit(1)
