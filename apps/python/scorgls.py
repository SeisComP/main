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

import os
import sys

import seiscomp.core
import seiscomp.client
import seiscomp.datamodel


def _parseTime(timestring):
    t = seiscomp.core.Time()
    if t.fromString(timestring, "%F %T"):
        return t
    if t.fromString(timestring, "%FT%T"):
        return t
    if t.fromString(timestring, "%FT%TZ"):
        return t
    if t.fromString(timestring, "%F"):
        return t
    return None


def readXML(self):
    ar = seiscomp.io.XMLArchive()
    if not ar.open(self._inputFile):
        print(f"Unable to open input file {self._inputFile}")
        return []

    obj = ar.readObject()
    if obj is None:
        raise TypeError("invalid input file format")

    ep = seiscomp.datamodel.EventParameters.Cast(obj)
    if ep is None:
        raise ValueError("no event parameters found in input file")

    originIDs = []
    for i in range(ep.originCount()):
        org = ep.origin(i)

        # check time requirements
        orgTime = org.time().value()
        if orgTime < self._startTime:
            continue
        if orgTime > self._endTime:
            continue

        # check author requirements
        if self.author:
            try:
                author = org.creationInfo().author()
            except:
                continue

            if author != self.author:
                continue

        try:
            originIDs.append(org.publicID())
        except:
            continue

    return originIDs


class OriginList(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = seiscomp.core.Time()
        self._endTime = seiscomp.core.Time.GMT()
        self._delimiter = None
        self._inputFile = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Name of input XML file. Read from stdin if '-' is given. Deactivates "
            "reading events from database",
        )
        self.commandline().addGroup("Origins")
        self.commandline().addStringOption(
            "Origins",
            "begin",
            "The lower bound of the time interval. Format: '1970-01-01 00:00:00'.",
        )
        self.commandline().addStringOption(
            "Origins",
            "end",
            "The upper bound of the time interval. Format: '1970-01-01 00:00:00'.",
        )
        self.commandline().addStringOption(
            "Origins", "author", "The author of the origins."
        )

        self.commandline().addGroup("Output")
        self.commandline().addStringOption(
            "Output",
            "delimiter,D",
            "The delimiter of the resulting " "origin IDs. Default: '\\n')",
        )
        return True

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        try:
            self._inputFile = self.commandline().optionString("input")
        except RuntimeError:
            pass

        if self._inputFile:
            self.setDatabaseEnabled(False, False)

        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            start = self.commandline().optionString("begin")
        except RuntimeError:
            start = "1900-01-01T00:00:00Z"

        self._startTime = _parseTime(start)
        if self._startTime is None:
            seiscomp.logging.error(f"Wrong 'begin' format '{start}'")
            return False
        seiscomp.logging.debug(f"Setting start to {self._startTime.toString('%FT%TZ')}")

        try:
            end = self.commandline().optionString("end")
        except RuntimeError:
            end = "2500-01-01T00:00:00Z"

        self._endTime = _parseTime(end)
        if self._endTime is None:
            seiscomp.logging.error(f"Wrong 'end' format '{end}'")
            return False
        seiscomp.logging.debug(f"Setting end to {self._endTime.toString('%FT%TZ')}")

        try:
            self.author = self.commandline().optionString("author")
            seiscomp.logging.debug(f"Filtering origins by author {self.author}")
        except RuntimeError:
            self.author = False

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except RuntimeError:
            self._delimiter = "\n"

        return True

    def printUsage(self):
        print(
            f"""Usage:
  {os.path.basename(__file__)} [options]

List origin IDs available in a given time range and print to stdout."""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            f"""Examples:
Print all origin IDs from year 2022 and thereafter
  {os.path.basename(__file__)} -d mysql://sysop:sysop@localhost/seiscomp --begin "2022-01-01 00:00:00"

Print IDs of all events in XML file
  {os.path.basename(__file__)} -i origins.xml
"""
        )

    def run(self):
        if self._inputFile:
            out = readXML(self)
            print(f"{self._delimiter.join(out)}\n", file=sys.stdout)
            return True

        seiscomp.logging.debug(f"Search interval: {self._startTime} - {self._endTime}")
        out = []
        q = (
            "select PublicObject.%s, Origin.* from Origin, PublicObject where Origin._oid=PublicObject._oid and Origin.%s >= '%s' and Origin.%s < '%s'"
            % (
                self.database().convertColumnName("publicID"),
                self.database().convertColumnName("time_value"),
                self.database().timeToString(self._startTime),
                self.database().convertColumnName("time_value"),
                self.database().timeToString(self._endTime),
            )
        )

        if self.author:
            q += " and Origin.%s = '%s' " % (
                self.database().convertColumnName("creationInfo_author"),
                self.query().toString(self.author),
            )

        for obj in self.query().getObjectIterator(
            q, seiscomp.datamodel.Origin.TypeInfo()
        ):
            org = seiscomp.datamodel.Origin.Cast(obj)
            if org:
                out.append(org.publicID())

        print(f"{self._delimiter.join(out)}\n", file=sys.stdout)
        return True


def main():
    app = OriginList(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
