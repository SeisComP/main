#!/usr/bin/env python3
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
import os
import re
import argparse
import seiscomp.core
import seiscomp.io


class MyArgumentParser(argparse.ArgumentParser):
    def format_epilog(self):
        return self.epilog


def str2time(timestring):
    """
    Liberally accept many time string formats and convert them to a
    seiscomp.core.Time
    """

    timestring = timestring.strip()
    for c in ["-", "/", ":", "T", "Z"]:
        timestring = timestring.replace(c, " ")
    timestring = timestring.split()
    assert 3 <= len(timestring) <= 6
    timestring.extend((6 - len(timestring)) * ["0"])
    timestring = " ".join(timestring)
    timeFormat = "%Y %m %d %H %M %S"
    if timestring.find(".") != -1:
        timeFormat += ".%f"

    time = seiscomp.core.Time()
    time.fromString(timestring, timeFormat)
    return time


def time2str(time):
    """
    Convert a seiscomp.core.Time to a string
    """
    return time.toString("%Y-%m-%d %H:%M:%S.%f000000")[:23]


def recordInput(filename=None, datatype=seiscomp.core.Array.INT):
    """
    Simple Record iterator that reads from a file (to be specified by
    filename) or -- if no filename was specified -- reads from standard input
    """

    stream = seiscomp.io.RecordStream.Create("file")
    if not stream:
        raise IOError("failed to create a RecordStream")

    if not filename:
        filename = "-"
    filenames = "-"

    if filename == "-":
        print(
            "Waiting for data input from stdin. Use Ctrl + C to interrupt.",
            file=sys.stderr,
        )
    else:
        if not os.path.exists(filename):
            print("Cannot find file {}".format(filename), file=sys.stderr)
            sys.exit()

    if not stream.setSource(filename):
        print("  + failed to assign source file to RecordStream", file=sys.stderr)
        sys.exit()

    records = seiscomp.io.RecordInput(stream, datatype, seiscomp.core.Record.SAVE_RAW)

    while True:
        try:
            record = next(records)
        except Exception:
            print("Received invalid or no input", file=sys.stderr)
            sys.exit()

        if not record:
            return
        yield record


tmin = str2time("1970-01-01 00:00:00")
tmax = str2time("2500-01-01 00:00:00")
ifile = "-"

description = (
    "Read unsorted and possibly multiplexed miniSEED files. "
    "Sort data by time (multiplexing) and filter the individual "
    "records by time and/or streams. Apply this before playbacks "
    "and waveform archiving."
)

epilog = (
    "Examples:\n"
    "Read data from multiple files, extract streams by time, sort records by start "
    "time, remove duplicate records\n"
    "  cat f1.mseed f2.mseed f3.mseed |\\\n"
    "  scmssort -v -t '2007-03-28 15:48~2007-03-28 16:18' -u > sorted.mseed\n"
    "\n"
    "Extract streams by time, stream code and sort records by end time\n"
    "  echo CX.PB01..BH? |\\ \n"
    "  scmssort -v -E -t '2007-03-28 15:48~2007-03-28 16:18' -u -l - test.mseed > "
    "sorted.mseed"
)


# p = MyArgumentParser(
#     usage="\n  %prog [options] [files | < ] > ", description=description, epilog=epilog
# )
p = MyArgumentParser(
    description=description,
    epilog=epilog,
    formatter_class=argparse.RawDescriptionHelpFormatter,
)
p.add_argument(
    "-E",
    "--sort-by-end-time",
    action="store_true",
    help="Sort according to record end time; default is start time.",
)
p.add_argument(
    "-r",
    "--rm",
    action="store_true",
    help="Remove all traces in stream list given by --list instead of keeping them.",
)
p.add_argument(
    "-l",
    "--list",
    action="store",
    help="File with stream list to filter the records. "
    "One stream per line. Instead of a file read the from stdin (-). "
    "Line format: NET.STA.LOC.CHA - wildcards and regular expressions "
    "are considered. Example: CX.*..BH?.",
)
p.add_argument(
    "-t",
    "--time-window",
    action="store",
    help="Specify time window (as one -properly quoted- string). Times "
    "are of course UTC and separated by a tilde '~'.",
)
p.add_argument(
    "-u",
    "--uniqueness",
    action="store_true",
    help="Ensure uniqueness of output, i.e. skip duplicate records.",
)
p.add_argument("-v", "--verbose", action="store_true", help="Run in verbose mode.")


if sys.stdin.isatty():
    p.add_argument(
        "filenames",
        nargs="+",
        help="Names of input files in miniSEED format.",
    )

opt = p.parse_args()
try:
    filenames = opt.filenames
except AttributeError:
    filenames = ["-"]

if opt.time_window:
    tmin, tmax = list(map(str2time, opt.time_window.split("~")))

if opt.verbose:
    print(
        "Considered time window: %s~%s" % (time2str(tmin), time2str(tmax)),
        file=sys.stderr,
    )

listFile = None
removeStreams = False
if opt.list:
    listFile = opt.list
    print("Considered stream list from: %s" % (listFile), file=sys.stderr)

    if opt.rm:
        removeStreams = True
        print("Removing listed streams", file=sys.stderr)


def _time(record):
    if opt.sort_by_end_time:
        return seiscomp.core.Time(record.endTime())
    return seiscomp.core.Time(record.startTime())


def _in_time_window(record, tMin, tMax):
    return record.endTime() >= tMin and record.startTime() <= tMax


def readStreamList(file):
    streamList = []

    try:
        if file == "-":
            f = sys.stdin
            file = "stdin"
        else:
            f = open(listFile, "r", encoding="utf-8")
    except FileNotFoundError:
        print("%s: error: unable to open" % listFile, file=sys.stderr)
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

        toks = line.split(".")
        if len(toks) != 4:
            f.close()
            print(
                "error: %s in line %d has invalid line format, expected "
                "stream list: NET.STA.LOC.CHA - 1 line per stream including "
                "regular expressions" % (listFile, lineNumber),
                file=sys.stderr,
            )
            return []

        streamList.append(line)

    f.close()

    if len(streamList) == 0:
        return []

    return streamList


if not filenames:
    filenames = ["-"]

streams = None
if listFile:
    streams = readStreamList(listFile)
    if not streams and not removeStreams:
        print(" + cannot extract data", file=sys.stderr)
        sys.exit()

    if opt.verbose:
        string = " + streams: "

        for stream in streams:
            string += stream + " "
        print("%s" % (string), file=sys.stderr)

    pattern = re.compile("|".join(streams))

readRecords = 0
networks = set()
stations = set()
locations = set()
channels = set()
readStreams = set()
outEnd = None
outStart = None

if filenames:
    first = None
    time_raw = []
    for fileName in filenames:
        if opt.verbose:
            print("Reading file '%s'" % fileName, file=sys.stderr)

        for rec in recordInput(fileName):
            if not rec:
                continue

            if not _in_time_window(rec, tmin, tmax):
                continue

            raw = rec.raw().str()
            streamCode = "%s.%s.%s.%s" % (
                rec.networkCode(),
                rec.stationCode(),
                rec.locationCode(),
                rec.channelCode(),
            )

            if listFile:
                foundStream = False

                if pattern.match(streamCode):
                    foundStream = True

                if removeStreams:
                    foundStream = not foundStream

                if not foundStream:
                    continue

            # collect statistics for verbosity mode
            if opt.verbose:
                networks.add(rec.networkCode())
                stations.add(rec.stationCode())
                locations.add(rec.locationCode())
                channels.add(rec.channelCode())
                readStreams.add(streamCode)
                readRecords += 1

                start = rec.startTime()
                end = rec.endTime()

                if (outStart is None) or (start < outStart):
                    outStart = seiscomp.core.Time(start)

                if (outEnd is None) or (end > outEnd):
                    outEnd = seiscomp.core.Time(end)

            t = _time(rec)
            if first is None:
                first = t
            t = float(t - first)  # float needs less memory
            time_raw.append((t, raw))

    if opt.verbose:
        print(
            " + %d networks, %d stations, %d sensor locations, "
            "%d channel codes, %d streams, %d records"
            % (
                len(networks),
                len(stations),
                len(locations),
                len(channels),
                len(readStreams),
                readRecords,
            ),
            file=sys.stderr,
        )
        print("Sorting records", file=sys.stderr)
    time_raw.sort()

    if opt.verbose:
        print("Writing output", file=sys.stderr)
    previous = None

    out = sys.stdout
    try:
        # needed in Python 3, fails in Python 2
        out = out.buffer
    except AttributeError:
        # assuming this is Python 2, nothing to be done
        pass

    duplicates = 0
    for item in time_raw:
        if item == previous:
            duplicates += 1
            if opt.uniqueness:
                continue

        t, raw = item
        out.write(raw)

        previous = item

    if opt.verbose:
        print("Finished", file=sys.stderr)
        if opt.uniqueness:
            print(
                "  + found and removed {} duplicate records".format(duplicates),
                file=sys.stderr,
            )
        else:
            if duplicates > 0:
                print(
                    "  + found {} duplicate records - remove with: scmssort -u".format(
                        duplicates
                    ),
                    file=sys.stderr,
                )
            else:
                print("  + found 0 duplicate records", file=sys.stderr)

        print("Output:", file=sys.stderr)
        if outStart and outEnd:
            print(
                " + time window: %s~%s"
                % (seiscomp.core.Time(outStart), seiscomp.core.Time(outEnd)),
                file=sys.stderr,
            )
        else:
            print("No data found in time window", file=sys.stderr)

    else:
        # This is an important hint which should always be printed
        if duplicates > 0 and not opt.uniqueness:
            print(
                "Found {} duplicate records - remove with: scmssort -u".format(
                    duplicates
                ),
                file=sys.stderr,
            )
