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
import seiscomp.core, seiscomp.client, seiscomp.datamodel, seiscomp.logging


def _parseTime(timestring):
    t = seiscomp.core.Time()
    if t.fromString(timestring, "%F %T"):
        return t
    if t.fromString(timestring, "%FT%T"):
        return t
    if t.fromString(timestring, "%FT%TZ"):
        return t
    return None


class EventList(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = None
        self._endTime = None
        self._delimiter = None
        self._modifiedAfterTime = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Events")
        self.commandline().addStringOption("Events", "begin",
                                           "specify the lower bound of the time interval")
        self.commandline().addStringOption("Events", "modified-after",
                                           "select events modified after the specified time")
        self.commandline().addStringOption(
            "Events", "end", "specify the upper bound of the time interval")
        self.commandline().addStringOption("Events", "delimiter,D",
                                           "specify the delimiter of the resulting event ids (default: '\\n')")
        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            start = self.commandline().optionString("begin")
        except:
            start = "1900-01-01T00:00:00Z"
        self._startTime = _parseTime(start)
        if self._startTime is None:
            seiscomp.logging.error("Wrong 'begin' format '%s'" % start)
            return False
        seiscomp.logging.debug("Setting start to %s" % self._startTime.toString("%FT%TZ"))

        try:
            end = self.commandline().optionString("end")
        except:
            end = "2500-01-01T00:00:00Z"
        self._endTime = _parseTime(end)
        if self._endTime is None:
            seiscomp.logging.error("Wrong 'end' format '%s'" % end)
            return False
        seiscomp.logging.debug("Setting end to %s" % self._endTime.toString("%FT%TZ"))

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except:
            self._delimiter = "\n"

        try:
            modifiedAfter = self.commandline().optionString("modified-after")
            self._modifiedAfterTime = _parseTime(modifiedAfter)
            if self._modifiedAfterTime is None:
                seiscomp.logging.error("Wrong 'modified-after' format '%s'" % modifiedAfter)
                return False
            seiscomp.logging.debug(
                    "Setting 'modified-after' time to %s" %
                    self._modifiedAfterTime.toString("%FT%TZ"))
        except:
            pass

        return True

    def run(self):
        first = True
        out = []
        for obj in self.query().getEvents(self._startTime, self._endTime):
            evt = seiscomp.datamodel.Event.Cast(obj)
            if not evt:
                continue

            if self._modifiedAfterTime is not None:
                try:
                    if evt.creationInfo().modificationTime() < self._modifiedAfterTime:
                        continue
                except ValueError:
                    continue

            out.append(evt.publicID())

        sys.stdout.write("%s\n" % self._delimiter.join(out))

        return True


def main():
    app = EventList(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
