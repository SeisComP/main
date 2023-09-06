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

from __future__ import print_function

from getopt import gnu_getopt, GetoptError
from collections import namedtuple

import bisect
import glob
import re
import time
import sys
import os

import seiscomp.core
import seiscomp.client
import seiscomp.config
import seiscomp.io
import seiscomp.system


class Archive:
    def __init__(self, archiveDirectory):
        self.archiveDirectory = archiveDirectory
        self.filePool = {}
        self.filePoolSize = 100

    def iterators(self, begin, end, net, sta, loc, cha):
        t = time.gmtime(begin.seconds())
        t_end = time.gmtime(end.seconds())

        start_year = t[0]

        for year in range(start_year, t_end[0] + 1):
            if year > start_year:
                begin = seiscomp.core.Time.FromYearDay(year, 1)
                t = time.gmtime(begin.seconds())

            if net == "*":
                netdir = self.archiveDirectory + str(year) + "/"
                try:
                    files = os.listdir(netdir)
                except BaseException:
                    print(
                        f"Info: skipping year {year} - not found in archive {netdir}",
                        file=sys.stderr,
                    )
                    continue

                its = []
                for file in files:
                    if not os.path.isdir(netdir + file):
                        continue
                    tmp_its = self.iterators(begin, end, file, sta, loc, cha)
                    for it in tmp_its:
                        its.append(it)

                return its

            if sta == "*":
                stadir = self.archiveDirectory + str(year) + "/" + net + "/"

                try:
                    files = os.listdir(stadir)
                except BaseException:
                    print(
                        f"Info: skipping network {net} - not found in archive {stadir}",
                        file=sys.stderr,
                    )
                    continue

                its = []
                for file in files:
                    if not os.path.isdir(stadir + file):
                        continue
                    tmp_its = self.iterators(begin, end, net, file, loc, cha)
                    for it in tmp_its:
                        its.append(it)

                return its

            # Check if cha contains a regular expression or not
            mr = re.match("[A-Z|a-z|0-9]*", cha)
            if (mr and mr.group() != cha) or cha == "*":
                cha = cha.replace("?", ".")
                stadir = self.archiveDirectory + str(year) + "/" + net + "/" + sta + "/"
                try:
                    files = os.listdir(stadir)
                except BaseException:
                    print(
                        f"Info: skipping station {sta} - no data files "
                        f"found in archive {stadir}",
                        file=sys.stderr,
                    )
                    return []

                its = []
                for file in files:
                    if not os.path.isdir(stadir + file):
                        print(
                            f"Info: skipping data file '{file}' - not found in archive "
                            f"{stadir}",
                            file=sys.stderr,
                        )
                        continue

                    part = file[:3]
                    if cha != "*":
                        mr = re.match(cha, part)
                        if not mr or mr.group() != part:
                            continue

                    tmp_its = self.iterators(begin, end, net, sta, loc, part)
                    for it in tmp_its:
                        its.append(it)

                return its

            if loc == "*":
                directory = (
                    self.archiveDirectory
                    + str(year)
                    + "/"
                    + net
                    + "/"
                    + sta
                    + "/"
                    + cha
                    + ".D/"
                )
                its = []

                start_day = t[7]
                if t_end[0] > year:
                    end_day = 366
                else:
                    end_day = t_end[7]

                files = glob.glob(directory + "*.%03d" % start_day)

                # Find first day with data
                while not files and start_day <= end_day:
                    start_day += 1
                    begin = seiscomp.core.Time.FromYearDay(year, start_day)
                    files = glob.glob(directory + "*.%03d" % start_day)

                if not files:
                    t = time.gmtime(begin.seconds() - 86400)
                    print(
                        f"Info: skipping streams '{net}.{sta}.*.{cha} on "
                        f"{time.strftime('%Y-%m-%d', t)}' - no data found for this day "
                        f"in archive {directory}",
                        file=sys.stderr,
                    )

                for file in files:
                    file = file.split("/")[-1]
                    if not os.path.isfile(directory + file):
                        print(
                            f"Info: skipping data file '{file}' - not found in archive "
                            f"{directory}",
                            file=sys.stderr,
                        )
                        continue

                    tmp_its = self.iterators(
                        begin, end, net, sta, file.split(".")[2], cha
                    )
                    for it in tmp_its:
                        its.append(it)

                return its

            it = StreamIterator(self, begin, end, net, sta, loc, cha)
            if it.record is not None:
                return [it]

        return []

    def location(self, rt, net, sta, loc, cha):
        t = time.gmtime(rt.seconds())
        directory = f"{str(t[0])}/{net}/{sta}/{cha}.D/"
        file = f"{net}.{sta}.{loc}.{cha}.D.{str(t[0])}.{t[7]:03d}"

        return directory, file

    def findIndex(self, begin, end, file):
        rs = seiscomp.io.FileRecordStream()
        rs.setRecordType("mseed")
        if not rs.setSource(self.archiveDirectory + file):
            return None, None

        ri = seiscomp.io.RecordInput(rs)

        index = None
        retRec = None

        for rec in ri:
            if rec is None:
                break

            if rec.samplingFrequency() <= 0:
                continue

            if rec.startTime() >= end:
                break
            if rec.endTime() < begin:
                continue

            index = rs.tell()
            retRec = rec
            break

        rs.close()

        return retRec, index

    def readRecord(self, file, index):
        try:
            rs = self.filePool[file]
        except BaseException:
            rs = seiscomp.io.FileRecordStream()
            rs.setRecordType("mseed")
            if not rs.setSource(self.archiveDirectory + file):
                return (None, None)

            rs.seek(index)

            # Remove old handles
            if len(self.filePool) < self.filePoolSize:
                # self.filePool.pop(self.fileList[-1])
                # print "Remove %s from filepool" % self.fileList[-1]
                # del self.fileList[-1]
                self.filePool[file] = rs

        ri = seiscomp.io.RecordInput(
            rs, seiscomp.core.Array.INT, seiscomp.core.Record.SAVE_RAW
        )
        # Read only valid records
        while True:
            rec = next(ri)
            if rec is None:
                break
            if rec.samplingFrequency() <= 0:
                continue
            break

        index = rs.tell()

        if rec is None:
            # Remove file handle from pool
            rs.close()
            try:
                self.filePool.pop(file)
            except BaseException:
                pass

        return rec, index

    def stepTime(self, rt):
        rt = rt + seiscomp.core.TimeSpan(86400)
        t = rt.get()
        rt.set(t[1], t[2], t[3], 0, 0, 0, 0)
        return rt


class StreamIterator:
    def __init__(self, ar, begin, end, net, sta, loc, cha):
        self.archive = ar

        self.begin = begin
        self.end = end

        self.net = net
        self.sta = sta
        self.loc = loc
        self.cha = cha

        self.compareEndTime = False

        workdir, file = ar.location(begin, net, sta, loc, cha)
        self.file = workdir + file
        # print "Starting at file %s" % self.file

        self.record, self.index = ar.findIndex(begin, end, self.file)
        if self.record:
            self.current = self.record.startTime()
            self.currentEnd = self.record.endTime()

    def next(self):
        # needed for Python 2 only
        return self.__next__()

    def __next__(self):
        while True:
            self.record, self.index = self.archive.readRecord(self.file, self.index)
            if self.record:
                self.current = self.record.startTime()
                self.currentEnd = self.record.endTime()
                if self.current >= self.end:
                    self.record = None

                return self.record

            # Skip the current day file
            self.current = self.archive.stepTime(self.current)
            # Are we out of scope?
            if self.current >= self.end:
                self.record = None
                return self.record

            # Use the new file and start from the beginning
            workdir, file = self.archive.location(
                self.current, self.net, self.sta, self.loc, self.cha
            )
            self.file = workdir + file
            self.index = 0

    def __cmp__(self, other):
        if self.compareEndTime:
            if self.currentEnd > other.currentEnd:
                return 1

            if self.currentEnd < other.currentEnd:
                return -1

            return 0

        if self.current > other.current:
            return 1

        if self.current < other.current:
            return -1

        return 0

    def __lt__(self, other):
        if self.__cmp__(other) < 0:
            return True
        return False


class ArchiveIterator:
    def __init__(self, ar, sortByEndTime):
        self.archive = ar
        self.streams = []
        self.sortByEndTime = sortByEndTime

    def append(self, beginTime, endTime, net, sta, loc, cha):
        its = self.archive.iterators(beginTime, endTime, net, sta, loc, cha)
        for it in its:
            it.compareEndTime = self.sortByEndTime
            bisect.insort(self.streams, it)

    def appendStation(self, beginTime, endTime, net, sta):
        self.append(beginTime, endTime, net, sta, "*", "*")

    def nextSort(self):
        if not self.streams:
            return None

        stream = self.streams.pop(0)

        rec = stream.record

        next(stream)

        if stream.record is not None:
            # Put the stream back on the right (sorted) position
            bisect.insort(self.streams, stream)

        return rec


class Copy:
    def __init__(self, archiveIterator):
        self.archiveIterator = archiveIterator

    def __iter__(self):
        for stream in self.archiveIterator.streams:
            rec = stream.record
            while rec:
                yield rec
                rec = next(stream)


class Sorter:
    def __init__(self, archiveIterator):
        self.archiveIterator = archiveIterator

    def __iter__(self):
        while True:
            rec = self.archiveIterator.nextSort()
            if not rec:
                return
            yield rec


RenameRule = namedtuple("RenameRule", "pattern newNet newSta newLoc newCha")


class RecordRenamer:
    def __init__(self):
        self.renameRules = []

    def addRule(self, rules):
        # split multiple comma separated rules
        for singleRule in rules.split(","):
            # A rule is [match-stream:]rename-stream
            matchStream = None
            renameStream = None
            token = singleRule.split(":")
            if len(token) == 2:  # a mathing stream rule is present
                matchStream = token[0]
                renameStream = token[1]
            else:
                renameStream = token[0]

            if matchStream is not None:
                if len(matchStream.split(".")) != 4:  # split in NET STA LOC CHA
                    print(
                        f"Error: check rename option syntax ({rules})", file=sys.stderr
                    )
                    return False

                # convert to a valid regular expression pattern
                matchStream = re.sub(r"\.", r"\.", matchStream)  # . becomes \.
                matchStream = re.sub(r"\?", ".", matchStream)  # ? becomes .
                matchStream = re.sub(r"\*", ".*", matchStream)  # * becomes.*
                matchStream = re.compile(matchStream)

            renameNslc = renameStream.split(".")  # split in NET STA LOC CHA
            if len(renameNslc) != 4:
                print(f"Error: check rename option syntax ({rules})", file=sys.stderr)
                return False

            r = RenameRule(
                matchStream, renameNslc[0], renameNslc[1], renameNslc[2], renameNslc[3]
            )
            self.renameRules.append(r)

        return True

    def printRules(self):
        for r in self.renameRules:
            print(
                "Renaming %s to %s.%s.%s.%s"
                % (
                    (r.pattern.pattern if r.pattern is not None else "*.*.*.*"),
                    r.newNet,
                    r.newSta,
                    r.newLoc,
                    r.newCha,
                ),
                file=sys.stderr,
            )

    def applyRules(self, rec):
        for rule in self.renameRules:
            if rule.pattern is None or rule.pattern.fullmatch(
                f"{rec.networkCode()}.{rec.stationCode()}."
                f"{rec.locationCode()}.{rec.channelCode()}"
            ):
                if rule.newNet != "-":
                    rec.setNetworkCode(rule.newNet)
                if rule.newSta != "-":
                    rec.setStationCode(rule.newSta)
                if rule.newLoc != "-":
                    rec.setLocationCode(rule.newLoc)
                if rule.newCha != "-":
                    if len(rule.newCha) == 3 and rule.newCha[2] == "-":
                        rec.setChannelCode(rule.newCha[0:2] + rec.channelCode()[2])
                    else:
                        rec.setChannelCode(rule.newCha)


####################################################################
##
# Application block
##
####################################################################


def checkFile(fileName):
    """
    Check the miniSEED records in a file, report unsorted records.

    Parameters
    ----------
    fileName : miniSEED
        Waveform file to check.

    Returns
    -------
    false
        If no error is found in file
    error string
        If file or records are corrupted

    """
    rs = seiscomp.io.FileRecordStream()
    rs.setRecordType("mseed")

    if not rs.setSource(fileName):
        return "cannot read file"

    ri = seiscomp.io.RecordInput(rs)
    lastEnd = None
    foundSortError = 0
    foundCountError = 0
    errorMsg = ""
    for rec in ri:
        if rec is None:
            continue

        if not rec.sampleCount():
            foundCountError += 1

        sF = rec.samplingFrequency()
        if sF <= 0:
            continue

        if lastEnd and rec.endTime() <= lastEnd:
            overlap = float(lastEnd - rec.endTime())

            if overlap >= 1 / sF:
                foundSortError += 1

        lastEnd = rec.endTime()

    if foundSortError:
        errorMsg += f"found {foundSortError} unordered records"

    if foundCountError:
        errorMsg += f"found {foundCountError} records without samples"

    if errorMsg:
        return errorMsg

    return False


def checkFilePrint(fileName, streamDict):
    """
    Check the miniSEED records in a file, report NSLC along with parameters

    Parameters
    ----------
    fileName : miniSEED
        Waveform file to check.

    Returns
    -------
    false
        If no error is found in file
    error string
        If file or records are corrupted

    """
    rs = seiscomp.io.FileRecordStream()
    rs.setRecordType("mseed")

    if not rs.setSource(fileName):
        return "cannot read file"

    ri = seiscomp.io.RecordInput(rs)
    for rec in ri:
        if rec is None:
            continue

        stream = f"{rec.networkCode()}.{rec.stationCode()}.{rec.locationCode()}.{rec.channelCode()}"
        recStart = rec.startTime()
        recEnd = rec.endTime()

        if stream in streamDict:
            streamStart, streamEnd, streamNRec, streamNSamp = streamDict[stream][:4]
            if recStart.valid() and recStart.iso() < streamStart:
                # update start time
                streamDict.update(
                    {
                        stream: (
                            recStart.iso(),
                            streamEnd,
                            streamNRec + 1,
                            streamNSamp + rec.data().size(),
                            rec.samplingFrequency(),
                        )
                    }
                )
            if recEnd.valid() and recEnd.iso() > streamEnd:
                # update end time
                streamDict.update(
                    {
                        stream: (
                            streamStart,
                            recEnd.iso(),
                            streamNRec + 1,
                            streamNSamp + rec.data().size(),
                            rec.samplingFrequency(),
                        )
                    }
                )
        else:
            # add stream for the first time
            streamDict[stream] = (
                recStart.iso(),
                recEnd.iso(),
                1,
                rec.data().size(),
                rec.samplingFrequency(),
            )

    return True


def str2time(timeString):
    """
    Liberally accept many time string formats and convert them to a
    seiscomp.core.Time
    """

    timeString = timeString.strip()
    for c in ["-", "/", ":", "T", "Z"]:
        timeString = timeString.replace(c, " ")

    toks = timeString.split()

    assert 3 <= len(toks) <= 6

    toks.extend((6 - len(toks)) * ["0"])
    toks = " ".join(toks)
    formatString = "%Y %m %d %H %M %S"
    if toks.find(".") != -1:
        formatString += ".%f"

    t = seiscomp.core.Time()
    t.fromString(toks, formatString)

    if not t.fromString(toks, formatString):
        raise ValueError()

    return t


def time2str(scTime):
    """
    Convert a seiscomp.core.Time to a string
    """
    return scTime.toString("%Y-%m-%d %H:%M:%S.%2f")


def create_dir(directory):
    if os.access(directory, os.W_OK):
        return True

    try:
        os.makedirs(directory)
        return True
    except BaseException:
        return False


def isFile(url):
    toks = url.split("://")
    return len(toks) < 2 or toks[0] == "file"


Stream = namedtuple("Stream", "net sta loc cha")


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

        toks = line.split(".")
        if len(toks) != 4:
            f.close()
            print(
                f"error: {listFile} in line {lineNumber} has invalid line format, "
                "expecting NET.STA.LOC.CHA - 1 line per stream",
                file=sys.stderr,
            )
            return []

        streams.append(Stream(toks[0], toks[1], toks[2], toks[3]))

    f.close()

    if len(streams) == 0:
        return []

    return streams


StreamTime = namedtuple("StreamTime", "tmin tmax net sta loc cha")


def readStreamTimeList(listFile):
    """
    Read list of streams with time windows

    Parameters
    ----------
    file : file
        Input list file, one line per stream
        format: 2007-03-28 15:48;2007-03-28 16:18;NET.STA.LOC.CHA

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
    except BaseException:
        print(f"error: unable to open {listFile}", file=sys.stderr)
        return []

    lineNumber = -1
    for line in f:
        lineNumber = lineNumber + 1
        line = line.strip()
        # ignore comments
        if not line or line[0] == "#":
            continue

        toks = line.split(";")
        if len(toks) != 3:
            f.close()
            print(
                f"{listFile}:{lineNumber}: error: invalid line format, expected 3 "
                "items separated by ';'",
                file=sys.stderr,
            )
            return []

        try:
            tMin = str2time(toks[0])
        except Exception:
            f.close()
            print(
                f"{listFile}:{lineNumber}: error: invalid time format (tmin)",
                file=sys.stderr,
            )
            return []

        try:
            tMax = str2time(toks[1])
        except Exception:
            f.close()
            print(
                f"{listFile}:{lineNumber}: error: invalid time format (tMax)",
                file=sys.stderr,
            )
            return []

        streamID = toks[2].strip()
        toks = streamID.split(".")
        if len(toks) != 4:
            f.close()
            print(
                f"{listFile}:{lineNumber}: error: invalid stream format",
                file=sys.stderr,
            )
            return []

        streams.append(StreamTime(tMin, tMax, toks[0], toks[1], toks[2], toks[3]))

    f.close()

    return streams


usage_info = """
Usage:
  scart -I [RecordStream] [options] [archive]
  scart -I [RecordStream] [options] --stdout
  scart -d [options] [archive]
  scart --check [options] [archive]

Import miniSEED waveforms or dump records from an SDS structure, sort them,
modify the time and replay them. Also check files and archives.
For Import and Dump mode the data streams can be selected in three ways
using the combinations of options: -n -c -t  or --nslc -t or --list

Verbosity:
  -h, --help       Display this help message.
  -v, --verbose    Print verbose information.

Mode:
  --check arg      Check mode: Check all files in the given directory structure
                   for erroneous miniSEED records. If no directory is given,
                   $SEISCOMP_ROOT/var/lib/archive is scanned. Checks are only
                   complete for files containing exactly one stream. More
                   complete checks are made with scmssort.
  -d, --dump       Export (dump) mode. Read from SDS archive.
  -I arg           Import mode (default): Specify the recordstream URL to read the data
                   from for archiving. When using any other recordstream than file, a
                   stream list file is needed. Specifying - implies file://- (stdin).
                   If no mode is explicitly specified, -I file://- is assumed.

Processing:
  -c arg            Channel filter to be applied to the data streams.
                    Default for Dump: "(B|E|H|M|S)(D|H|L|N)(E|F|N|Z|1|2|3)"
                    Default for Import: "*"
  -E                Dump mode: Sort according to record end time; default is start time.
  --files arg       Dump mode: Specify the file handles to cache; default: 100
  -i, --ignore      Ignore records without data samples.
  -l, --list arg    Import, dump mode: Use a stream list file with time windows instead
                    of defined networks, channels and time window (-n, -c and -t are
                    ignored).
                    The list can be generated from events by scevtstreams. One
                    line per stream. Line format: starttime;endtime;streamID
                        2007-03-28 15:48;2007-03-28 16:18;GE.LAST.*.*
                        2007-03-28 15:48;2007-03-28 16:18;GE.PMBI..BH?
  -m, --modify      Dump mode: Modify the record time for real time playback
                    when dumping.
  -n arg            Import, dump mode: Data stream selection as a comma separated
                    list "stream1,stream2,streamX" where each stream can be NET or
                    NET.STA or NET.STA.LOC or NET.STA.LOC.CHA. If CHA is omitted,
                    it defaults to the value of -c option. Default: "*".
  --nslc arg        Import, dump mode: Stream list file to be used instead of
                    defined networks and channels (-n and -c are ignored) for
                    filtering the data by the given streams. Dump mode: Use in
                    combination with -t!
                    One line per stream, line format: NET.STA.LOC.CHA
  --rename arg      Import, dump mode: Rename stream data according to the
                    provided rule(s).
                    A rule is "[match-stream:]rename-stream" and match-stream
                    is optional. match-stream and rename-stream are in the
                    "NET.STA.LOC.CHA" format. match-stream supports special
                    charactes "?" "*" "|" "(" ")". rename-stream supports the
                    special character "-" that can be used in place of NET, STA,
                    LOC, CHA codes with the meaning of not renaming those.
                    "-" can also be used as the last character in CHA code.
                    Multiple rules can be provided as a comma separated list
                    or by providing multiple --rename options.
  -s, --sort        Dump mode: Sort records.
  --speed arg       Dump mode: Specify the speed to dump the records. A value
                    of 0 means no delay. Otherwise speed is a multiplier of
                    the real time difference between the records.
  -t t1~t2          Import, dump mode: UTC time window filter to be applied to
                    the data streams in the format: "StartTime~EndTime"
                    e.g. "2022-12-20 12:00:00~2022-12-23 14:00:10".

Output:
  -o, --output arg  Import mode: Write data to given file instead of creating
                    a SDS archive. Deactivates --stdout. Deactivated by --test.
  --print-streams   Import, dump, check mode: Print stream information to
                    stderr in addition to all other output. Output:
                    NET.STA.LOC.CHA StartTime EndTime records samples samplingRate.
  --stdout          Import mode: Write to stdout instead of creating a SDS
                    archive. Deactivated by --test and --output.
  --test            Test input only, deactivate all miniSEED output. This switch
                    is useful for debugging and printing stream information with
                    --print-streams.
  --with-filecheck  Import mode: Check all accessed files after import. Unsorted
                    or unreadable files are reported to stderr. Checks are only
                    complete for files containing exactly one stream. More 
                    complete checks are made with scmssort.
  --with-filename   Import mode: Print all accessed files to sterr after import.

Examples:
Read from /archive, create a miniSEED file where records are sorted by end time
  scart -dsv -t '2022-03-28 15:48~2022-03-28 16:18' /archive > sorted.mseed

Import miniSEED data from file [your file], create a SDS archive
  scart -I file.mseed $SEISCOMP_ROOT/var/lib/archive

Import miniSEED data into a SDS archive, check all modified files for errors
  scart -I file.mseed --with-filecheck $SEISCOMP_ROOT/var/lib/archive

Import miniSEED data from FDSNWS into a SDS archive for specific time range and streams
  scart -I fdsnws://geofon.gfz-potsdam.de -t -t '2022-03-28 15:48~2022-03-28 16:18' --nslc list.file $SEISCOMP_ROOT/var/lib/archive

Check an archive for files with out-of order records
  scart --check /archive
"""


def usage(exitcode=0):
    print(usage_info, file=sys.stderr)
    sys.exit(exitcode)


def main():
    try:
        opts, files = gnu_getopt(
            sys.argv[1:],
            "I:dismEn:c:t:l:o:hv",
            [
                "stdout",
                "with-filename",
                "with-filecheck",
                "dump",
                "ignore",
                "list=",
                "nslc=",
                "sort",
                "modify",
                "speed=",
                "files=",
                "verbose",
                "test",
                "help",
                "check",
                "print-streams",
                "rename=",
                "output=",
            ],
        )
    except GetoptError:
        usage(exitcode=1)

    tmin = None
    tmax = None
    endtime = False
    verbose = False
    sort = False
    modifyTime = False
    dump = False
    importMode = False
    listFile = None
    nslcFile = None
    printStreams = False
    withFilename = False  # Whether to output accessed files for import or not
    checkFiles = False  # Check if output files are sorted by time
    checkSDS = False  # check the SDS archive for errors in files
    test = False
    filePoolSize = 100
    # default = stdin
    recordURL = "file://-"

    speed = 0
    stdout = False
    outputFile = None
    ignoreRecords = False

    channels = None
    networks = "*"

    archiveDirectory = "./"

    recordRenamer = RecordRenamer()

    for flag, arg in opts:
        if flag == "-t":
            try:
                tmin, tmax = list(map(str2time, arg.split("~")))
            except Exception:
                print(f"error: {arg}", file=sys.stderr)
                print(
                    "       Provide correct time interval: -t 'startTime~endtime'\n"
                    "       with valid time format, e.g.: 'YYYY-MM-DD hh:mm:ss'",
                    file=sys.stderr,
                )

                return 1

        elif flag == "-E":
            endtime = True
        elif flag in ["-h", "--help"]:
            usage(exitcode=0)
        elif flag in ["--check"]:
            checkSDS = True
        elif flag in ["--stdout"]:
            stdout = True
        elif flag in ["-o", "--output"]:
            outputFile = arg
        elif flag in ["--with-filename"]:
            withFilename = True
        elif flag in ["--with-filecheck"]:
            checkFiles = True
        elif flag in ["-v", "--verbose"]:
            verbose = True
        elif flag in ["-d", "--dump"]:
            dump = True
        elif flag in ["-i", "--ignore"]:
            ignoreRecords = True
        elif flag in ["-l", "--list"]:
            listFile = arg
        elif flag in ["--nslc"]:
            nslcFile = arg
        elif flag in ["--rename"]:
            if not recordRenamer.addRule(arg):
                return -1
        elif flag in ["--print-streams"]:
            printStreams = True
        elif flag in ["-s", "--sort"]:
            sort = True
        elif flag in ["-m", "--modify"]:
            modifyTime = True
        elif flag in ["--speed"]:
            speed = float(arg)
        elif flag in ["--files"]:
            filePoolSize = int(arg)
        elif flag in ["--test"]:
            test = True
        elif flag == "-I":
            recordURL = arg
            importMode = True
        elif flag == "-n":
            networks = arg
        elif flag == "-c":
            channels = arg
        else:
            usage(exitcode=1)

    if not dump and not checkSDS and not importMode:
        importMode = True

    if files:
        archiveDirectory = files[0]
    else:
        try:
            archiveDirectory = os.environ["SEISCOMP_ROOT"] + "/var/lib/archive"
        except BaseException:
            pass

    if outputFile:
        stdout = False

    if checkSDS and dump:
        print("Stopping: either use '-d' or '--check'", file=sys.stderr)
        return -1

    if checkSDS and importMode:
        print("Stopping: either use '-I' or '--check'", file=sys.stderr)
        return -1

    if dump and importMode:
        print("Stopping: either use '-d' or '-I'", file=sys.stderr)
        return -1

    try:
        if archiveDirectory[-1] != "/":
            archiveDirectory = archiveDirectory + "/"
    except BaseException:
        pass

    if (
        not test
        and not stdout
        and not outputFile
        and not os.path.isdir(archiveDirectory)
    ):
        print(
            f"Info: archive directory '{archiveDirectory}' not found - stopping",
            file=sys.stderr,
        )
        return -1

    # Import and Dump mode require either -l or -t option. The only exception is
    # when a file is given in input to Import mode where the time window is
    # optional
    if not listFile and (dump or (importMode and not isFile(recordURL))):
        if not tmin or not tmax:
            print(
                "Info: provide a time window with '-t' when '--list' is "
                "not used - stopping",
                file=sys.stderr,
            )
            return -1

        if tmin >= tmax:
            print(
                f"Info: start time '{time2str(tmin)}' after end time '{time2str(tmax)}'"
                " - stopping",
                file=sys.stderr,
            )
            return -1

    archive = Archive(archiveDirectory)
    archive.filePoolSize = filePoolSize

    if verbose:
        seiscomp.logging.enableConsoleLogging(seiscomp.logging.getAll())

        if tmin and tmax:
            print(f"Time window: {time2str(tmin)}~{time2str(tmax)}", file=sys.stderr)
        if listFile:
            print(f"Stream file: '{listFile}'", file=sys.stderr)
        elif nslcFile:
            print(f"Stream file: '{nslcFile}'", file=sys.stderr)

        if dump:
            if not sort and not modifyTime:
                print("Mode: DUMP", file=sys.stderr)
            elif sort and not modifyTime:
                print("Mode: DUMP & SORT", file=sys.stderr)
            elif not sort and modifyTime:
                print("Mode: DUMP & MODIFY_TIME", file=sys.stderr)
            elif sort and modifyTime:
                print("Mode: DUMP & SORT & MODIFY_TIME", file=sys.stderr)
            print(f"Archive: {archiveDirectory}", file=sys.stderr)

        if checkSDS:
            print("Mode: Check", file=sys.stderr)

        if importMode:
            print("Mode: IMPORT", file=sys.stderr)
            if not stdout and not outputFile:
                print(f"Archive: {archiveDirectory}", file=sys.stderr)

        recordRenamer.printRules()

    archiveIterator = ArchiveIterator(archive, endtime)

    if checkSDS:
        dump = False
        stdout = False

    if dump:
        stdout = True

    if stdout:
        out = sys.stdout.buffer

    if not test and outputFile:
        print(f"Output data to file: {outputFile}", file=sys.stderr)
        try:
            out = open(outputFile, "wb")
        except Exception:
            print("Cannot create output file {outputFile}", file=sys.stderr)
            return -1

    # list file witht times takes priority over nslc list
    if listFile:
        nslcFile = None

    if channels is None:
        if dump:
            channels = "(B|E|H|M|S)(D|H|L|N)(E|F|N|Z|1|2|3)"
        else:  # RecordStream doesn't support that complex syntax for channels
            channels = "*"

    # Populate streams for both Dump and Import mode
    streams = []
    if listFile:
        streamFilter = readStreamTimeList(listFile)
        for stream in streamFilter:
            if stream.tmin >= stream.tmax:
                print(
                    f"Info: ignoring {stream.net}.{stream.sta}.{stream.loc}.{stream.cha} - "
                    f"start {stream.tmin} after end {stream.tmax}",
                    file=sys.stderr,
                )
                continue
            streams.append(stream)

    elif nslcFile:
        streamFilter = readStreamList(nslcFile)
        for stream in streamFilter:
            streams.append(
                StreamTime(tmin, tmax, stream.net, stream.sta, stream.loc, stream.cha)
            )

    elif not checkSDS:
        if networks == "*":
            streams.append(StreamTime(tmin, tmax, "*", "*", "*", channels))
        else:
            items = networks.split(",")
            for n in items:
                n = n.strip()
                nsl = n.split(".")
                if len(nsl) == 1:
                    streams.append(StreamTime(tmin, tmax, nsl[0], "*", "*", channels))
                elif len(nsl) == 2:
                    streams.append(
                        StreamTime(tmin, tmax, nsl[0], nsl[1], "*", channels)
                    )
                elif len(nsl) == 3:
                    streams.append(
                        StreamTime(tmin, tmax, nsl[0], nsl[1], nsl[2], channels)
                    )
                elif len(nsl) == 4:
                    streams.append(
                        StreamTime(tmin, tmax, nsl[0], nsl[1], nsl[2], nsl[3])
                    )
                else:
                    print(
                        "error: wrong format of -n option - stopping", file=sys.stderr
                    )
                    return -1

    streamDict = {}
    if dump:
        for stream in streams:
            archiveIterator.append(
                stream.tmin, stream.tmax, stream.net, stream.sta, stream.loc, stream.cha
            )
            if verbose:
                print(
                    f"Adding stream to list: {stream.net}.{stream.sta}.{stream.loc}."
                    f"{stream.cha} {stream.tmin} - {stream.tmax}",
                    file=sys.stderr,
                )
        stime = None
        realTime = seiscomp.core.Time.GMT()

        if sort:
            records = Sorter(archiveIterator)
        else:
            records = Copy(archiveIterator)

        foundRecords = 0
        foundCountError = 0
        for rec in records:
            # skip corrupt records
            if not rec.sampleCount():
                foundCountError += 1
                if ignoreRecords:
                    continue

            etime = seiscomp.core.Time(rec.endTime())

            if stime is None:
                stime = etime
                if verbose:
                    print(f"First record: {stime.iso()}", file=sys.stderr)

            dt = etime - stime

            now = seiscomp.core.Time.GMT()

            if speed > 0:
                playTime = (realTime + dt).toDouble() / speed
            else:
                playTime = now.toDouble()

            sleepTime = playTime - now.toDouble()
            if sleepTime > 0:
                time.sleep(sleepTime)

            if modifyTime:
                recLength = etime - rec.startTime()
                rec.setStartTime(seiscomp.core.Time(playTime) - recLength)

            if verbose:
                etime = rec.endTime()
                print(
                    f"{rec.streamID()} time current: "
                    f"{seiscomp.core.Time.LocalTime().iso()} start: "
                    f"{rec.startTime().iso()} end: {etime.iso()}",
                    file=sys.stderr,
                )

            if printStreams:
                stream = f"{rec.networkCode()}.{rec.stationCode()}.{rec.locationCode()}.{rec.channelCode()}"
                recStart = rec.startTime()
                recEnd = rec.endTime()

                if stream in streamDict:
                    streamStart = streamDict[stream][0]
                    streamEnd = streamDict[stream][1]
                    streamNRec = streamDict[stream][2]
                    streamNSamp = streamDict[stream][3]
                    if recStart.valid() and recStart.iso() < streamStart:
                        # update start time
                        streamDict.update(
                            {
                                stream: (
                                    recStart.iso(),
                                    streamEnd,
                                    streamNRec + 1,
                                    streamNSamp + rec.data().size(),
                                    rec.samplingFrequency(),
                                )
                            }
                        )
                    if recEnd.valid() and recEnd.iso() > streamEnd:
                        # update end time
                        streamDict.update(
                            {
                                stream: (
                                    streamStart,
                                    recEnd.iso(),
                                    streamNRec + 1,
                                    streamNSamp + rec.data().size(),
                                    rec.samplingFrequency(),
                                )
                            }
                        )
                else:
                    # add stream for the first time
                    streamDict[stream] = (
                        recStart.iso(),
                        recEnd.iso(),
                        1,
                        rec.data().size(),
                        rec.samplingFrequency(),
                    )

            recordRenamer.applyRules(rec)

            if not test:
                out.write(rec.raw().str())

            foundRecords += 1

        if verbose:
            print(f"Found records: {foundRecords}", file=sys.stderr)

            if test:
                print("Test mode: no records written", file=sys.stderr)

        if foundCountError:
            print(f"Found {foundCountError} empty records", file=sys.stderr)
            if ignoreRecords:
                print("  + which are ignored and not written", file=sys.stderr)
            else:
                print("  + which are written", file=sys.stderr)

    elif checkSDS:
        foundIssues = 0
        checkedFiles = 0
        for path, subdirs, files in os.walk(archiveDirectory):
            for name in files:
                fileName = os.path.join(path, name)
                checkedFiles += 1

                if printStreams:
                    # only collect stream IDs
                    checkFilePrint(fileName, streamDict)

                issueFound = checkFile(fileName)
                if issueFound:
                    foundIssues += 1
                    print(f"{fileName} has an issue", file=sys.stderr)
                    print(f"  + {issueFound}", file=sys.stderr)

        print(f"Found issues in {foundIssues}/{checkedFiles} files", file=sys.stderr)

    elif importMode:  # Import mode
        env = seiscomp.system.Environment.Instance()
        cfg = seiscomp.config.Config()
        env.initConfig(cfg, "scart")
        try:
            plugins = cfg.getStrings("plugins")
            registry = seiscomp.system.PluginRegistry.Instance()
            for p in plugins:
                registry.addPluginName(p)
            registry.loadPlugins()
        except Exception:
            pass

        rs = seiscomp.io.RecordStream.Open(recordURL)
        if rs is None:
            print(
                f"Unable to open recordstream '{recordURL}'",
                file=sys.stderr,
            )
            return -1

        if not rs.setRecordType("mseed"):
            print(
                f"Format 'mseed' is not supported by recordstream '{recordURL}'",
                file=sys.stderr,
            )
            return -1

        # Add time/stream selections to recordstream
        for stream in streams:
            done = False
            # If the input is a file, then the time window is not mandatory
            if stream.tmin is None and stream.tmax is None and isFile(recordURL):
                if stream.net == stream.sta == stream.loc == stream.cha == "*":
                    # skip the default *.*.*.* filter (redundant) because old
                    # versions of File RecordStream do not support wildcards
                    done = True
                else:
                    done = rs.addStream(stream.net, stream.sta, stream.loc, stream.cha)
            else:
                done = rs.addStream(
                    stream.net,
                    stream.sta,
                    stream.loc,
                    stream.cha,
                    stream.tmin,
                    stream.tmax,
                )
            if not done:
                print(
                    f"error: adding stream: {stream.tmin} {stream.tmax} "
                    f"{stream.net}.{stream.sta}.{stream.loc}.{stream.cha}",
                    file=sys.stderr,
                )
            elif verbose:
                print(
                    f"adding stream: {stream.tmin} {stream.tmax} "
                    f"{stream.net}.{stream.sta}.{stream.loc}.{stream.cha}",
                    file=sys.stderr,
                )

        inputRecord = seiscomp.io.RecordInput(
            rs, seiscomp.core.Array.INT, seiscomp.core.Record.SAVE_RAW
        )

        filePool = {}
        outdir = None
        f = None
        accessedFiles = set()
        if outputFile:
            accessedFiles.add(outputFile)
        foundCountError = 0
        foundRecords = False

        try:
            for rec in inputRecord:
                if not foundRecords:
                    foundRecords = True

                if not rec.sampleCount():
                    foundCountError += 1
                    if ignoreRecords:
                        continue

                if printStreams:
                    stream = f"{rec.networkCode()}.{rec.stationCode()}.{rec.locationCode()}.{rec.channelCode()}"
                    recStart = rec.startTime()
                    recEnd = rec.endTime()

                    if stream in streamDict:
                        streamStart, streamEnd, streamNRec, streamNSamp = streamDict[
                            stream
                        ][:4]
                        if recStart.valid() and recStart.iso() < streamStart:
                            # update start time
                            streamDict.update(
                                {
                                    stream: (
                                        recStart.iso(),
                                        streamEnd,
                                        streamNRec + 1,
                                        streamNSamp + rec.data().size(),
                                        rec.samplingFrequency(),
                                    )
                                }
                            )
                        if recEnd.valid() and recEnd.iso() > streamEnd:
                            # update end time
                            streamDict.update(
                                {
                                    stream: (
                                        streamStart,
                                        recEnd.iso(),
                                        streamNRec + 1,
                                        streamNSamp + rec.data().size(),
                                        rec.samplingFrequency(),
                                    )
                                }
                            )
                    else:
                        # add stream for the first time
                        streamDict[stream] = (
                            recStart.iso(),
                            recEnd.iso(),
                            1,
                            rec.data().size(),
                            rec.samplingFrequency(),
                        )

                recordRenamer.applyRules(rec)

                if stdout or outputFile:
                    if verbose:
                        print(
                            f"{rec.streamID()} {rec.startTime().iso()}", file=sys.stderr
                        )
                    if not test:
                        out.write(rec.raw().str())
                    continue

                directory, file = archive.location(
                    rec.startTime(),
                    rec.networkCode(),
                    rec.stationCode(),
                    rec.locationCode(),
                    rec.channelCode(),
                )
                file = directory + file

                if not test:
                    try:
                        f = filePool[file]
                    except BaseException:
                        outdir = "/".join((archiveDirectory + file).split("/")[:-1])
                        if not create_dir(outdir):
                            print(
                                f"Could not create directory '{outdir}'",
                                file=sys.stderr,
                            )
                            return -1

                        try:
                            f = open(archiveDirectory + file, "ab")
                        except BaseException:
                            print(
                                f"File {archiveDirectory + file} could not be opened for writing",
                                file=sys.stderr,
                            )
                            return -1

                        # Remove old handles
                        if len(filePool) < filePoolSize:
                            filePool[file] = f

                    if withFilename or checkFiles:
                        accessedFiles.add(archiveDirectory + file)

                    f.write(rec.raw().str())

                if verbose:
                    print(f"{rec.streamID()} {rec.startTime().iso()} {file}")
        except Exception as e:
            print(f"Exception: {e}")

        if not foundRecords:
            print("Found no records - check your input")
            return -1

        if foundCountError:
            print(f"Input has {foundCountError} empty records", file=sys.stderr)
            if ignoreRecords:
                print("  + which are ignored and not written", file=sys.stderr)
            else:
                print("  + which are ignored and not written", file=sys.stderr)

        if test:
            print(
                "Test mode: Found errors were stated above, if any",
                file=sys.stderr,
            )

        else:
            if outputFile:
                out.close()
                if verbose:
                    print(
                        f"Records were written to file: {outputFile}", file=sys.stderr
                    )

            if outdir:
                print(
                    f"Records were written to archive: {archiveDirectory}",
                    file=sys.stderr,
                )

            if checkFiles:
                print(
                    "Testing accessed files (may take some time):",
                    file=sys.stderr,
                )
                foundIssues = 0
                checkedFiles = 0
                for fileName in accessedFiles:
                    checkedFiles += 1
                    issueFound = checkFile(fileName)
                    if issueFound:
                        foundIssues += 1
                        print(f"{fileName} has an issue", file=sys.stderr)
                        print(f"  + {issueFound}", file=sys.stderr)

                print(
                    f"Found issues in {foundIssues}/{checkedFiles} files",
                    file=sys.stderr,
                )

            if withFilename:
                print("List of accessed files:", file=sys.stderr)
                for fileName in accessedFiles:
                    print(fileName, file=sys.stderr)

    if printStreams and streamDict:
        minTime = seiscomp.core.Time.GMT()
        maxTime = str2time("1970-01-01 00:00:00")
        totalRecs = 0
        totalSamples = 0
        totalChans = set()
        totalNetworks = set()
        totalStations = set()

        print(
            "# streamID       start                       end                         "
            "records samples samplingRate",
            file=sys.stderr,
        )
        for key, (start, end, nRecs, nSamples, sps) in sorted(streamDict.items()):
            print(
                f"{key: <{16}} {start: <{27}} {end: <{27}} {nRecs} {nSamples} {sps}",
                file=sys.stderr,
            )

            if maxTime < str2time(end):
                maxTime = str2time(end)

            if minTime > str2time(start):
                minTime = str2time(start)

            totalChans.add(key)
            totalNetworks.add(key.split(".")[0])
            totalStations.add(f"{key.split('.')[0]}.{key.split('.')[1]}")
            totalRecs += nRecs
            totalSamples += nSamples

        print(
            "# Summary",
            file=sys.stderr,
        )
        print(
            f"#   time range: {minTime.iso()} - {maxTime.iso()}",
            file=sys.stderr,
        )
        print(
            f"#   networks:   {len(totalNetworks)}",
            file=sys.stderr,
        )
        print(
            f"#   stations:   {len(totalStations)}",
            file=sys.stderr,
        )
        print(
            f"#   streams:    {len(totalChans)}",
            file=sys.stderr,
        )
        print(
            f"#   records:    {totalRecs}",
            file=sys.stderr,
        )
        print(
            f"#   samples:    {totalSamples}",
            file=sys.stderr,
        )


if __name__ == "__main__":
    sys.exit(main())
