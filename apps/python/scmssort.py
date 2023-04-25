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

import argparse
import os
import re
import sys
import traceback

from seiscomp import core, io

VERBOSITY = 0

INFO = 1
DEBUG = 2
TRACE = 3


def log(level, msg):
    print(f"[{level}] {msg}", file=sys.stderr)


def info_enabled():
    return VERBOSITY >= INFO


def debug_enabled():
    return VERBOSITY >= DEBUG


def trace_enabled():
    return VERBOSITY >= TRACE


def error(msg):
    log("error", msg)


def warning(msg):
    log("warning", msg)


def info(msg):
    if info_enabled():
        log("info", msg)


def debug(msg):
    if debug_enabled():
        log("debug", msg)


def trace(msg):
    if trace_enabled():
        log("trace", msg)


def parse_args():
    description = (
        "Read unsorted and possibly multiplexed miniSEED files. Sort data by time "
        "(multiplexing) and filter the individual records by time and/or streams. "
        "Apply this before playbacks and waveform archiving."
    )

    epilog = """Examples:
Read data from multiple files, extract streams by time, sort records by start time, \
ignore duplicated and empty records
  cat f1.mseed f2.mseed f3.mseed | \
scmssort -v -t '2007-03-28 15:48~2007-03-28 16:18' -ui > sorted.mseed

Extract streams by time, stream code and sort records by end time
  echo CX.PB01..BH? | \
scmssort -v -E -t '2007-03-28 15:48~2007-03-28 16:18' \
-u -l - test.mseed > sorted.mseed
"""

    p = argparse.ArgumentParser(
        description=description,
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "file",
        nargs="*",
        default="-",
        help="miniSEED file(s) to sort. If no file name or '-' is specified then "
        "standard input is used.",
    )
    p.add_argument(
        "-E",
        "--sort-by-end-time",
        action="store_true",
        help="Sort according to record end time; default is start time.",
    )
    p.add_argument(
        "-i",
        "--ignore",
        action="store_true",
        help="Ignore all records which have no data samples.",
    )
    p.add_argument(
        "-l",
        "--list",
        action="store",
        help="Filter records by a list of stream codes specified in a file or on stdin "
        "(-). One stream per line of format: NET.STA.LOC.CHA - wildcards and regular "
        "expressions are considered. Example: CX.*..BH?.",
    )
    p.add_argument(
        "-o",
        "--output",
        action="store",
        help="Name of output file for miniSEED data (default is stdout).",
    )
    p.add_argument(
        "-r",
        "--rm",
        action="store_true",
        help="Remove all traces in stream list given by '--list' instead of keeping "
        "them.",
    )
    p.add_argument(
        "-t",
        "--time-window",
        action="store",
        help="Time window to filter the records, format: <START TIME> ~ <END TIME>. "
        "Time values are in UTC, must start with an ISO date and may include time "
        "components starting on the hour down to milliseconds. Example: "
        "2023-01-15T12:15",
    )
    p.add_argument(
        "-u",
        "--uniqueness",
        action="store_true",
        help="Ensure uniqueness of output by skipping duplicate records.",
    )
    p.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Run in verbose mode. This option may be repeated several time to "
        "increase the level of verbosity. Example: -vvv.",
    )

    opt = p.parse_args()

    global VERBOSITY
    VERBOSITY += int(opt.verbose)

    if opt.rm and not opt.list:
        error("The '--rm' requires the '--list' option to be present as well.")
        sys.exit(1)

    return opt


def rec2id(record):
    return (
        f"{record.networkCode()}.{record.stationCode()}."
        f"{record.locationCode()}.{record.channelCode()}"
    )


def str2time(string):
    """
    Liberally accept many time string formats and convert them to a seiscomp.core.Time
    """

    if not string:
        return None

    string = string.strip()
    if not string:
        return None

    for c in ["-", "/", ":", "T", "Z"]:
        string = string.replace(c, " ")

    toks = string.split()
    if len(toks) < 3:
        raise ValueError("Too few date components")
    if len(toks) > 6:
        raise ValueError("Too many date/time components")

    toks.extend((6 - len(toks)) * ["0"])
    string = " ".join(toks)
    time_format = "%Y %m %d %H %M %S"
    if "." in string:
        time_format += ".%f"

    time = core.Time()
    if not time.fromString(string, time_format):
        raise ValueError(f"Invalid time format: {string}")

    return time


def time2str(time):
    """
    Convert a seiscomp.core.Time to a string
    """
    if not time:
        return ""

    return time.toString("%Y-%m-%d %H:%M:%S.%f000")[:23]


def read_time_window(opt):
    if not opt.time_window:
        return None, None

    try:
        toks = opt.time_window.split("~")
        if len(toks) != 2:
            if len(toks) < 2:
                raise ValueError("No tilde (~) found")
            raise ValueError("To many tilde (~) found")

        start, end = map(str2time, toks)
        info(f"Filtering records by time window: {time2str(start)}~{time2str(end)}")
    except Exception as e:
        error(f"Could not read time window: {e}")
        if debug_enabled():
            debug(traceback.format_exc())
        sys.exit(1)

    return start, end


def read_lines(file):
    # read from stdin
    if file == "-":
        for line in sys.stdin:
            yield line
        return

    # read from file
    with open(file, "r", encoding="utf-8") as f:
        for line in f:
            yield line


def compile_stream_pattern(opt):
    if not opt.list:
        return None

    streams = []
    pattern = None
    try:
        line_number = -1
        for line in map(str.strip, read_lines(opt.list)):
            line_number += 1

            # ignore empty lines and comments
            if not line or line.startswith("#"):
                continue

            toks = line.split(".")
            if len(toks) != 4:
                raise ValueError(
                    f"Invalid stream definition at line {line_number}. Expected the 4 "
                    "stream components NET.STA.LOC.CHA separated by a dot, "
                    "got: {line}."
                )

            streams.append(line)

        if not streams:
            raise ValueError("No stream definition found.")

        pattern = re.compile("|".join(streams))

    except Exception as e:
        error(f"Could not compile pattern from stream list file '{opt.list}': {e}")
        if debug_enabled():
            debug(traceback.format_exc())
        sys.exit(1)

    info(
        f"Using stream id {'DENY' if opt.rm else 'ALLOW'} list with {len(streams)} "
        "stream masks"
    )

    if debug_enabled():
        masks = "\n  + ".join(streams)
        debug(f"Stream masks:\n  + {masks}")

    return pattern


def record_input(file, datatype=core.Array.INT):
    """
    Simple record iterator that reads from a file (or stdin in case of '-')
    """
    stream = io.RecordStream.Create("file")
    if not stream:
        raise IOError("Failed to create a RecordStream")

    if file != "-" and not os.path.exists(file):
        raise FileNotFoundError("Could not find file")

    if not stream.setSource(file):
        raise Exception("Could not set record stream source")

    it = io.RecordInput(stream, datatype, core.Record.SAVE_RAW)

    if trace_enabled():
        while True:
            record = it.next()
            if not record:
                return

            trace(
                f"    + {time2str(record.startTime())}~{time2str(record.endTime())} "
                f"{rec2id(record)}"
            )
            yield record
    else:
        while True:
            record = it.next()
            if not record:
                return

            yield record


def unique(sequence):
    seen = set()
    return [x for x in sequence if not (x in seen or seen.add(x))]


def main():
    # parse commandline
    opt = parse_args()

    # time window
    t_min, t_max = read_time_window(opt)

    # stream filter
    pattern = compile_stream_pattern(opt)

    outputFile = None
    if opt.output:
        outputFile = opt.output

    # record buffer to be sorted later on, each item is a tuple of
    # (delta_time, raw_binary_record_data)
    rec_buf = []

    # statistics
    records_read = 0
    records_window = 0
    records_empty = 0

    # statistics (info mode)
    networks = set()
    stations = set()
    streams = set()
    buf_min = None
    buf_max = None

    # make sure to read from stdin only once
    files = [x for x in opt.file if x != "-"]
    if len(files) == len(opt.file):
        info(f"Reading data from {len(opt.file)} file(s)")
    elif not files:
        files = "-"
        info("Reading data from stdin. Use Ctrl + C to interrupt.")
    else:
        info(
            f"Reading data from stdin and {len(files)} files. Use Ctrl + C to "
            "interrupt."
        )
        files.insert(opt.file.index("-"), "-")

    # time or first valid record use as reference for sorting
    ref_time = None

    # read records from input file
    for file in files:
        records_file = 0
        records_empty_file = 0

        try:
            for rec in record_input(file):
                records_file += 1

                # skip record if outside time window
                if (t_min and rec.endTime() < t_min) or (
                    t_max and rec.startTime() > t_max
                ):
                    continue

                if pattern or info_enabled():
                    records_window += 1
                    stream_id = rec2id(rec)

                if pattern and bool(pattern.match(stream_id)) == bool(opt.rm):
                    continue

                if not rec.sampleCount():
                    trace(
                        f"    + found empty record staring at {time2str(rec.startTime())} "
                        f"{rec2id(rec)}"
                    )
                    records_empty_file += 1
                    if opt.ignore:
                        trace("      + ignored")
                        continue

                # record time reference set to start or end time depending on sort
                # option
                t = rec.endTime() if opt.sort_by_end_time else rec.startTime()

                if ref_time is None:
                    ref_time = core.Time(t)
                    t = 0
                else:
                    t = float(t - ref_time)  # float needs less memory

                # buffer tuple of (time delta, binary record data)
                rec_buf.append((t, rec.raw().str()))

                # collect statistics for debug mode
                if info_enabled():
                    networks.add(rec.networkCode())
                    stations.add(f"{rec.networkCode()}.{rec.stationCode()}")
                    streams.add(stream_id)
                    # copy of time object is required because record may be freed before
                    if not buf_min or rec.startTime() < buf_min:
                        buf_min = core.Time(rec.startTime())
                    if not buf_max or rec.startTime() > buf_max:
                        buf_max = core.Time(rec.endTime())

            name = "<stdin>" if file == "-" else file
            empty = f", empty: {records_empty_file}" if records_empty_file else ""
            debug(f"  + {name}: {records_file} records{empty}")

        except Exception as e:
            error(f"Could not read file '{file}: {e}")
            if debug_enabled():
                debug(traceback.format_exc())
            return 1

        records_read += records_file
        records_empty += records_empty_file

    # stop if no records have been read
    if not records_read:
        warning("No records found in input file(s).")
        return 0

    buf_len = len(rec_buf)

    # statistics about records read and filtered
    if info_enabled() and buf_len != records_read:
        info(
            f"""{records_read-buf_len}/{records_read} records filtered:
  + by time window: {records_read-records_window}
  + by stream id {'DENY' if opt.rm else 'ALLOW'} list: {records_window-buf_len}"""
        )

    # stop if no record passed the filter
    if not buf_len:
        warning("All records filtered, nothing to write.")
        return 0

    # network, station and stream information
    if info_enabled():
        info(
            f"Found data for {len(networks)} networks, {len(stations)} stations "
            f"and {len(streams)} streams",
        )
        if debug_enabled() and streams:
            streamList = "\n  + ".join(streams)
            debug(f"streams:\n  + {streamList}")

    # sort records by time only
    if buf_len > 1:
        info(f"Sorting {buf_len} records")
        rec_buf.sort()

    # write sorted records, count duplicates and optional remove them
    info(f"Writing {buf_len} records")
    prev_rec = None
    duplicates = 0

    if outputFile:
        print(f"Output data to file: {outputFile}", file=sys.stderr)
        try:
            out = open(outputFile, "wb")
        except Exception:
            print("Cannot create output file {outputFile}", file=sys.stderr)
            return -1
    else:
        out = sys.stdout.buffer

    for _t, rec in rec_buf:
        if rec == prev_rec:
            duplicates += 1
            if opt.uniqueness:
                continue
        else:
            prev_rec = rec

        out.write(rec)

    # statistics about records written
    if info_enabled():
        records_written = buf_len - duplicates if opt.uniqueness else buf_len
        msg = f"""Wrote {records_written} records
  + time window: {time2str(buf_min)}~{time2str(buf_max)}"""

        if opt.uniqueness:
            msg += f"""
  + found and removed {duplicates} duplicate records"""
        elif not duplicates:
            msg += """
  + no duplicate records found"""

        if opt.ignore:
            msg += f"""
  + {records_empty} empty records found and ignored"""

        info(msg)

    # additional warning output
    if records_empty and not opt.ignore:
        warning(f"Found {records_empty} empty records - remove with: scmssort -i")

    # This is an important hint which should always be printed
    if duplicates > 0 and not opt.uniqueness:
        warning(f"Found {duplicates} duplicate records - remove with: scmssort -u")

    return 0


if __name__ == "__main__":
    sys.exit(main())
