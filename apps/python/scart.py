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

from getopt import getopt, GetoptError
import bisect
import glob
import re
import time
import sys
import os
if sys.version_info < (2, 4):
    from sets import Set as set

import seiscomp.core
import seiscomp.client
import seiscomp.config
import seiscomp.io
import seiscomp.logging
import seiscomp.system


class Archive:
    def __init__(self, archiveDirectory):
        self.archiveDirectory = archiveDirectory
        self.filePool = dict()
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
                    sys.stderr.write(
                        "info: skipping year %i - not found in archive %s\n" %
                        (year, netdir))
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
                    sys.stderr.write(
                        "info: skipping network '%s' - not found in archive %s\n" %
                        (net, stadir))
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
                cha = cha.replace('?', '.')
                stadir = self.archiveDirectory + \
                    str(year) + "/" + net + "/" + sta + "/"
                try:
                    files = os.listdir(stadir)
                except BaseException:
                    sys.stderr.write(
                        "info: skipping station %s - no data files "
                        "found in archive %s\n" %
                        (sta, stadir))
                    return []

                its = []
                for file in files:
                    if not os.path.isdir(stadir + file):
                        sys.stderr.write(
                            "info: skipping data file '%s' - not found in archive %s\n" %
                            (file, stadir))
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
                dir = self.archiveDirectory + \
                    str(year) + "/" + net + "/" + sta + "/" + cha + ".D/"
                its = []

                start_day = t[7]
                if t_end[0] > year:
                    end_day = 366
                else:
                    end_day = t_end[7]

                files = glob.glob(dir + "*.%03d" % start_day)

                # Find first day with data
                while not files and start_day <= end_day:
                    start_day += 1
                    begin = seiscomp.core.Time.FromYearDay(year, start_day)
                    files = glob.glob(dir + "*.%03d" % start_day)

                if not files:
                    t = time.gmtime(begin.seconds() - 86400)
                    sys.stderr.write(
                        "info: skipping streams '%s.%s.*.%s on %s '"
                        "- no data found for this day in archive %s\n" %
                        (net, sta, cha, time.strftime(
                            "%Y-%m-%d", t), dir))

                for file in files:
                    file = file.split('/')[-1]
                    if not os.path.isfile(dir + file):
                        sys.stderr.write(
                            "info: skipping data file '%s' - not found in archive %s\n" %
                            (file, dir))
                        continue

                    tmp_its = self.iterators(
                        begin, end, net, sta, file.split('.')[2], cha)
                    for it in tmp_its:
                        its.append(it)

                return its

            it = StreamIterator(self, begin, end, net, sta, loc, cha)
            if it.record is not None:
                return [it]

        return []

    def location(self, rt, net, sta, loc, cha):
        t = time.gmtime(rt.seconds())
        dir = str(t[0]) + "/" + net + "/" + sta + "/" + cha + ".D/"
        file = net + "." + sta + "." + loc + "." + \
            cha + ".D." + str(t[0]) + ".%03d" % t[7]
        return dir, file

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
                #print "Remove %s from filepool" % self.fileList[-1]
                #del self.fileList[-1]
                self.filePool[file] = rs

        ri = seiscomp.io.RecordInput(
            rs, seiscomp.core.Array.INT, seiscomp.core.Record.SAVE_RAW)
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
        #print "Starting at file %s" % self.file

        self.record, self.index = ar.findIndex(begin, end, self.file)
        if self.record:
            self.current = self.record.startTime()
            self.currentEnd = self.record.endTime()

    def next(self):
        # needed for Python 2 only
        return self.__next__()

    def __next__(self):
        while True:
            self.record, self.index = self.archive.readRecord(
                self.file, self.index)
            if self.record:
                self.current = self.record.startTime()
                self.currentEnd = self.record.endTime()
                if self.current >= self.end:
                    self.record = None
                return self.record
            else:
                # Skip the current day file
                self.current = self.archive.stepTime(self.current)
                # Are we out of scope?
                if self.current >= self.end:
                    self.record = None
                    return self.record

                # Use the new file and start from the beginning
                workdir, file = self.archive.location(
                    self.current, self.net, self.sta, self.loc, self.cha)
                self.file = workdir + file
                self.index = 0
                #print "Continue at " + self.file

    def __cmp__(self, other):
        if self.compareEndTime:
            if self.currentEnd > other.currentEnd:
                return 1
            elif self.currentEnd < other.currentEnd:
                return -1
            return 0
        else:
            if self.current > other.current:
                return 1
            elif self.current < other.current:
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


####################################################################
##
# Application block
##
####################################################################


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
    format = "%Y %m %d %H %M %S"
    if timestring.find(".") != -1:
        format += ".%f"

    t = seiscomp.core.Time()
    t.fromString(timestring, format)
    return t


def time2str(time):
    """
    Convert a seiscomp.core.Time to a string
    """
    return time.toString("%Y-%m-%d %H:%M:%S.%2f")


def create_dir(dir):
    if os.access(dir, os.W_OK):
        return True

    try:
        os.makedirs(dir)
        return True
    except BaseException:
        return False


def isFile(url):
    toks = url.split('://')
    return len(toks) < 2 or toks[0] == "file"


def readStreamList(file):
    streams = []

    try:
        if file == "-":
            f = sys.stdin
            file = "stdin"
        else:
            f = open(listFile, 'r')
    except BaseException:
        sys.stderr.write("%s: error: unable to open\n" % listFile)
        return []

    lineNumber = -1
    for line in f:
        lineNumber = lineNumber + 1
        line = line.strip()
        # ignore comments
        if not line or line[0] == '#':
            continue

        toks = line.split(';')
        if len(toks) != 3:
            f.close()
            sys.stderr.write(
                "%s:%d: error: invalid line format, expected 3 "
                "items separated by ';'\n" %
                (listFile, lineNumber))
            return []

        try:
            tmin = str2time(toks[0].strip())
        except BaseException:
            f.close()
            sys.stderr.write(
                "%s:%d: error: invalid time format (tmin)\n" %
                (listFile, lineNumber))
            return []

        try:
            tmax = str2time(toks[1].strip())
        except BaseException:
            f.close()
            sys.stderr.write(
                "%s:%d: error: invalid time format (tmax)\n" %
                (listFile, lineNumber))
            return []

        streamID = toks[2].strip()
        toks = streamID.split('.')
        if len(toks) != 4:
            f.close()
            sys.stderr.write("%s:%d: error: invalid stream format\n" %
                             (listFile, lineNumber))
            return []

        streams.append((tmin, tmax, toks[0], toks[1], toks[2], toks[3]))

    f.close()

    return streams


usage_info = '''
Usage: 
  scart [options] [archive]

Import miniSEED waveforms or dump records from an SDS structure, sort them,
modify the time and replay them.

Verbosity:
  -v, --verbose    Print verbose information.
  -h, --help       Display this help message.

Mode:
  -d, --dump       Export (dump) mode. Read from SDS archive.
  -I               Import mode: Specify a recordstream URL when in import mode.
                   When using another recordstream than file a
                   stream list file is needed.
                   Default: file://- (stdin)

Output:
  --stdout         Writes on stdout if import mode is used instead
                   of creating a SDS archive
  --with-filename  Print all accessed files to stdout after import.
  -t t1~t2         Specify time window (as one properly quoted string)
                   times are of course UTC and separated by a tilde '~' .
  -l, --list       Use a stream list file instead of defined networks and
                   channels (-n and -c are ignored). The list can be generated
                   from events by scevtstreams.
                   line format: starttime;endtime;streamID
                                2007-03-28 15:48;2007-03-28 16:18;GE.LAST.*.*
                                2007-03-28 15:48;2007-03-28 16:18;GE.PMBI..BH?
  -s, --sort       Sort records.
  -m, --modify     Modify the record time for realtime playback when dumping.
      --speed      Specify the speed to dump the records
                   a value of 0 means no delay otherwise speed is a multiplier
                   of the real time difference between the records
  -n               Network code list (comma separated). Default: *
  -c               Channel filter (regular expression).
                   Default: "(B|E|H|M|S)(D|H|L|N)(E|F|N|Z|1|2|3)"
  -E               Sort according to record end time; default is start time
  --files          Specify the file handles to cache; default: 100
  --test           Test mode, no record output.

Examples:
Read from /archive, create a miniSEED file where records are sorted by end time
  scart -dsv -t '2007-03-28 15:48~2007-03-28 16:18' /archive > sorted.mseed

Import miniSEED data from file [your file] and create an SDS archive
  scart -I file://[your file] $SEISCOMP_ROOT/var/lib/archive

'''

def usage(exitcode=0):
    sys.stderr.write(usage_info)
    sys.exit(exitcode)


try:
    opts, files = getopt(sys.argv[1:], "I:dsmEn:c:t:l:hv",
                         ["stdout", "with-filename", "dump", "list=", "sort",
                          "modify", "speed=", "files=", "verbose", "test", "help"])
except GetoptError as e:
    usage(exitcode=1)


endtime = False
verbose = False
sort = False
modifyTime = False
dump = False
listFile = None
withFilename = False  # Whether to output accessed files for import or not
test = False
filePoolSize = 100
# default = stdin
recordURL = "file://-"

speed = 0
stdout = False

channels = "(B|E|H|M|S)(D|H|L|N)(E|F|N|Z|1|2|3)"
networks = "*"

archiveDirectory = "./"


for flag, arg in opts:
    if flag == "-t":
        tmin, tmax = list(map(str2time, arg.split("~")))
    elif flag == "-E":
        endtime = True
    elif flag in ["-h", "--help"]:
        usage(exitcode=0)
    elif flag in ["--stdout"]:
        stdout = True
    elif flag in ["--with-filename"]:
        withFilename = True
    elif flag in ["-v", "--verbose"]:
        verbose = True
    elif flag in ["-d", "--dump"]:
        dump = True
    elif flag in ["-l", "--list"]:
        listFile = arg
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
    elif flag == "-n":
        networks = arg
    elif flag == "-c":
        channels = arg
    else:
        usage(exitcode=1)


if files:
    archiveDirectory = files[0]
else:
    try:
        archiveDirectory = os.environ["SEISCOMP_ROOT"] + "/var/lib/archive"
    except BaseException:
        pass

try:
    if archiveDirectory[-1] != '/':
        archiveDirectory = archiveDirectory + '/'
except BaseException:
    pass

if not stdout and not os.path.isdir(archiveDirectory):
    sys.stderr.write("info: archive directory '%s' not found - stopping\n" %
                     archiveDirectory)
    sys.exit(-1)

archive = Archive(archiveDirectory)
archive.filePoolSize = filePoolSize

if verbose:
    seiscomp.logging.enableConsoleLogging(seiscomp.logging.getAll())
    if dump and not listFile:
        sys.stderr.write("Time window: %s~%s\n" %
                         (time2str(tmin), time2str(tmax)))
    sys.stderr.write("Archive: %s\n" % archiveDirectory)
    if dump:
        if not sort and not modifyTime:
            sys.stderr.write("Mode: DUMP\n")
        elif sort and not modifyTime:
            sys.stderr.write("Mode: DUMP & SORT\n")
        elif not sort and modifyTime:
            sys.stderr.write("Mode: DUMP & MODIFY_TIME\n")
        elif sort and modifyTime:
            sys.stderr.write("Mode: DUMP & SORT & MODIFY_TIME\n")
    else:
        sys.stderr.write("Mode: IMPORT\n")

archiveIterator = ArchiveIterator(archive, endtime)

if dump:
    stdout = True

if stdout:
    out = sys.stdout
    try:
        # needed in Python 3, fails in Python 2
        out = out.buffer
    except AttributeError:
        # assuming this is Python 2, nothing to be done
        pass

if dump:
    if listFile:
        streams = readStreamList(listFile)
        for stream in streams:
            if verbose:
                sys.stderr.write("adding stream: %s.%s.%s.%s\n" % (
                    stream[2], stream[3], stream[4], stream[5]))
            archiveIterator.append(
                stream[0], stream[1], stream[2],
                stream[3], stream[4], stream[5])
    else:
        if networks == "*":
            archiveIterator.append(tmin, tmax, "*", "*", "*", channels)
        else:
            items = networks.split(",")
            for n in items:
                n = n.strip()
                archiveIterator.append(tmin, tmax, n, "*", "*", channels)

    stime = None
    realTime = seiscomp.core.Time.GMT()

    if sort:
        records = Sorter(archiveIterator)
    else:
        records = Copy(archiveIterator)

    for rec in records:
        # skip corrupt records
        etime = seiscomp.core.Time(rec.endTime())

        if stime is None:
            stime = etime
            if verbose:
                sys.stderr.write("First record: %s\n" % stime.iso())

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
            sys.stderr.write(
                "%s %s %s %s\n" %
                (rec.streamID(),
                 seiscomp.core.Time.LocalTime().iso(),
                 rec.startTime().iso(),
                 etime.iso()))

        if not test:
            out.write(rec.raw().str())

else:
    env = seiscomp.system.Environment.Instance()
    cfg = seiscomp.config.Config()
    env.initConfig(cfg, "scart")
    try:
        plugins = cfg.getStrings("plugins")
        registry = seiscomp.system.PluginRegistry.Instance()
        for p in plugins:
            registry.addPluginName(p)
        registry.loadPlugins()
    except Exception as e:
        pass

    rs = seiscomp.io.RecordStream.Open(recordURL)
    if rs is None:
        sys.stderr.write("Unable to open recordstream '%s'\n" % recordURL)
        sys.exit(-1)

    if not rs.setRecordType("mseed"):
        sys.stderr.write(
            "Format 'mseed' is not supported by recordstream '%s'\n" %
            recordURL)
        sys.exit(-1)

    if not isFile(recordURL):
        if not listFile:
            sys.stderr.write(
                "A stream list is needed to fetch data from another source than a file\n")
            sys.exit(-1)

        streams = readStreamList(listFile)
        for stream in streams:
            # Add stream to recordstream
            if not rs.addStream(
                    stream[2],
                    stream[3],
                    stream[4],
                    stream[5],
                    stream[0],
                    stream[1]):
                if verbose:
                    sys.stderr.write(
                        "error: adding stream: %s %s %s.%s.%s.%s\n" %
                        (stream[0], stream[1], stream[2], stream[3], stream[4], stream[5]))
            else:
                if verbose:
                    sys.stderr.write(
                        "adding stream: %s %s %s.%s.%s.%s\n" %
                        (stream[0], stream[1], stream[2], stream[3], stream[4], stream[5]))

    input = seiscomp.io.RecordInput(
        rs, seiscomp.core.Array.INT, seiscomp.core.Record.SAVE_RAW)
    filePool = dict()
    f = None
    accessedFiles = set()
    try:
        for rec in input:
            if stdout:
                out.write(rec.raw().str())
                continue

            dir, file = archive.location(rec.startTime(), rec.networkCode(
            ), rec.stationCode(), rec.locationCode(), rec.channelCode())
            file = dir + file

            if not test:
                try:
                    f = filePool[file]
                except BaseException:
                    outdir = '/'.join((archiveDirectory +
                                       file).split('/')[:-1])
                    if not create_dir(outdir):
                        sys.stderr.write(
                            "Could not create directory '%s'\n" % outdir)
                        sys.exit(-1)

                    try:
                        f = open(archiveDirectory + file, 'ab')
                    except BaseException:
                        sys.stderr.write(
                            "File '%s' could not be opened for writing\n" %
                            (archiveDirectory + file))
                        sys.exit(-1)

                    # Remove old handles
                    if len(filePool) < filePoolSize:
                        filePool[file] = f

                if withFilename:
                    accessedFiles.add(file)
                f.write(rec.raw().str())
            else:
                if withFilename:
                    accessedFiles.add(file)

            if verbose:
                sys.stderr.write("%s %s %s\n" %
                                 (rec.streamID(), rec.startTime().iso(), file))
    except Exception as e:
        sys.stderr.write("Exception: %s\n" % str(e))

    if withFilename:
        for filename in accessedFiles:
            print(filename)
