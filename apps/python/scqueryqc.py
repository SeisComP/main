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

import os
import sys
import seiscomp.core
import seiscomp.client
import seiscomp.io
import seiscomp.datamodel

qcParamsDefault = (
    "latency,delay,timing,offset,rms,availability,"
    "'gaps count','gaps interval','gaps length',"
    "'overlaps count','overlaps interval','overlaps length',"
    "'spikes count','spikes interval','spikes amplitude'"
)


class WfqQuery(seiscomp.client.Application):
    def __init__(self, argc, argv):
        super().__init__(argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setLoggingToStdErr(True)
        self.setDaemonEnabled(False)

        self._streams = []
        self._fromInventory = False
        self._outfile = "-"
        self._parameter = qcParamsDefault
        self._start = "1900-01-01T00:00:00Z"
        self._end = None
        self._formatted = False

    def createCommandLineDescription(self):
        self.commandline().addGroup("Output")
        self.commandline().addStringOption(
            "Output",
            "output,o",
            "output file name for XML. Writes to stdout if not given.",
        )
        self.commandline().addOption("Output", "formatted,f", "write formatted XML")

        self.commandline().addGroup("Query")
        self.commandline().addStringOption(
            "Query",
            "begin,b",
            "Begin time of query. Uses 1900-01-01T00:00:00 unless given.",
        )
        self.commandline().addStringOption(
            "Query",
            "end,e",
            "End time of query. Uses current time unless given.",
        )
        self.commandline().addStringOption(
            "Query",
            "stream-id,i",
            "Waveform stream ID to search for QC parameters: net.sta.loc.cha -"
            " [networkCode].[stationCode].[sensorLocationCode].[channelCode]. "
            "Provide a single ID or a comma-separated list. Overrides "
            "--streams-from-inventory.",
        )
        self.commandline().addStringOption(
            "Query",
            "parameter,p",
            "QC parameter to output: (e.g. delay, rms, 'gaps count', ...). "
            "Provide a single parameter or a comma-separated list. Defaults "
            "apply if parameter is not given.",
        )
        self.commandline().addOption(
            "Query",
            "streams-from-inventory",
            "Read streams from inventory. Superseded by stream-id.",
        )

        return True

    def printUsage(self):
        print(
            f"""Usage:
  {os.path.basename(__file__)} [options]

Query a database for waveform quality control (QC) parameters.""",
            file=sys.stdout,
        )

        seiscomp.client.Application.printUsage(self)

        print(
            f"""Default QC parameters: {qcParamsDefault}

Examples:
Query rms and delay values for streams 'AU.AS18..SHZ' and 'AU.AS19..SHZ' from \
2021-11-20 00:00:00 until current
  {os.path.basename(__file__)} -d localhost -b 2021-11-20 -p rms,'gaps count' \
-i AU.AS18..SHZ,AU.AS19..SHZ""",
            file=sys.stdout,
        )

    def validateParameters(self):
        if not super().validateParameters():
            return False

        try:
            self._streams = [
                s.strip()
                for s in self.commandline().optionString("stream-id").split(",")
                if s.strip()
            ]
        except Exception:
            pass

        self._fromInventory = self.commandline().hasOption("streams-from-inventory")

        if not self._streams and not self._fromInventory:
            print(
                "Provide streamID(s): --stream-id or --streams-from-inventory",
                file=sys.stderr,
            )
            return False

        for stream in self._streams:
            if stream.count(".") != 3 or any(c in stream for c in "*?"):
                print(
                    f"Invalid stream ID (net.sta.loc.cha, no wildcards): {stream}",
                    file=sys.stderr,
                )
                return False

        # load inventory if no stream list is given
        self.setLoadInventoryEnabled(not self._streams)

        try:
            self._outfile = self.commandline().optionString("output")
        except Exception:
            print("No output file name given: Sending to stdout", file=sys.stderr)

        try:
            self._start = self.commandline().optionString("begin")
        except Exception:
            print(
                f"No begin time given, considering: {self._start}",
                file=sys.stderr,
            )

        try:
            self._end = self.commandline().optionString("end")
        except Exception:
            print("No end time given, considering 'now'", file=sys.stderr)

        try:
            self._parameter = self.commandline().optionString("parameter")
        except Exception:
            print("No QC parameter given, using default", file=sys.stderr)

        # "'gaps count'" -> "gaps count"
        self._parameter = [
            p.strip().strip("'\"")
            for p in self._parameter.split(",")
            if p.strip().strip("'\"")
        ]

        self._formatted = self.commandline().hasOption("formatted")

        start = seiscomp.core.Time.FromString(self._start)
        if start is None:
            print(f"Wrong 'begin' format '{self._start}'", file=sys.stderr)
            return False

        if self._end is None:
            end = seiscomp.core.Time.UTC()
        else:
            end = seiscomp.core.Time.FromString(self._end)
            if end is None:
                print(f"Wrong 'end' format '{self._end}'", file=sys.stderr)
                return False

        if start > end:
            print("Begin time is after end time", file=sys.stderr)
            return False

        self._start, self._end = start, end
        return True

    def _isActive(self, obj):
        if obj.start() > self._end:
            return False
        try:
            return obj.end() >= self._start
        except ValueError:
            return True

    def streamsFromInventory(self):
        inv = seiscomp.client.Inventory.Instance().inventory()
        if not inv:
            return []

        streams = set()
        for inet in range(inv.networkCount()):
            net = inv.network(inet)
            for ista in range(net.stationCount()):
                sta = net.station(ista)
                if not self._isActive(sta):
                    continue
                for iloc in range(sta.sensorLocationCount()):
                    loc = sta.sensorLocation(iloc)
                    if not self._isActive(loc):
                        continue
                    for icha in range(loc.streamCount()):
                        cha = loc.stream(icha)
                        if self._isActive(cha):
                            streams.add(
                                f"{net.code()}.{sta.code()}.{loc.code()}.{cha.code()}"
                            )

        return sorted(streams)

    def run(self):
        if not self.query():
            print("No database connection!\n", file=sys.stderr)
            return False

        streams = self._streams or self.streamsFromInventory()
        if not streams:
            print("Empty stream list", file=sys.stderr)
            return False

        print("Request:", file=sys.stderr)
        print(f"  streams:           {streams}", file=sys.stderr)
        print(f"  number of streams: {len(streams)}", file=sys.stderr)
        print(f"  begin time:        {self._start.iso()}", file=sys.stderr)
        print(f"  end time:          {self._end.iso()}", file=sys.stderr)
        print(f"  parameters:        {self._parameter}", file=sys.stderr)
        print("Output:", file=sys.stderr)
        print(f"  file:              {self._outfile}", file=sys.stderr)
        print(f"  formatted XML:     {self._formatted}", file=sys.stderr)

        ar = seiscomp.io.XMLArchive()
        if not ar.create(self._outfile, True, True):
            print(f"Unable to write XML to {self._outfile}!\n", file=sys.stderr)
            return False

        qc = seiscomp.datamodel.QualityControl()

        # write parameters
        for parameter in self._parameter:
            for stream in streams:
                net, sta, loc, cha = stream.split(".")
                for obj in self.query().getWaveformQuality(
                    seiscomp.datamodel.WaveformStreamID(net, sta, loc, cha, ""),
                    parameter,
                    self._start,
                    self._end,
                ):
                    wfq = seiscomp.datamodel.WaveformQuality.Cast(obj)
                    if wfq:
                        qc.add(wfq)

        ar.setFormattedOutput(self._formatted)
        ar.writeObject(qc)
        ar.close()
        return True


def main():
    app = WfqQuery(len(sys.argv), sys.argv)
    return app()


if __name__ == "__main__":
    sys.exit(main())
