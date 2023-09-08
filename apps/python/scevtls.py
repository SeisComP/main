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
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.logging


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


class EventList(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = None
        self._endTime = None
        self.hours = None
        self._delimiter = None
        self._modifiedAfterTime = None
        self._preferredOrigin = False

    def createCommandLineDescription(self):
        self.commandline().addGroup("Events")
        self.commandline().addStringOption("Events", "begin",
                                           "Specify the lower bound of the "
                                           "time interval.")
        self.commandline().addStringOption("Events", "end",
                                           "Specify the upper bound of the "
                                           "time interval.")
        self.commandline().addStringOption("Events", "hours",
                                           "Start searching given hours before"
                                           " now. If set, --begin and --end "
                                           "are ignored.")
        self.commandline().addStringOption("Events", "modified-after",
                                           "Select events modified after the "
                                           "specified time.")

        self.commandline().addGroup("Output")
        self.commandline().addStringOption("Output", "delimiter,D",
                                           "Specify the delimiter of the "
                                           "resulting event IDs. "
                                           "Default: '\\n')")
        self.commandline().addOption("Output", "preferred-origin,p",
                                     "Print the ID of the preferred origin "
                                     "along with the event ID.")
        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            self.hours = float(self.commandline().optionString("hours"))
        except RuntimeError:
            pass

        end = "2500-01-01T00:00:00Z"
        if self.hours is None:
            try:
                start = self.commandline().optionString("begin")
            except RuntimeError:
                start = "1900-01-01T00:00:00Z"

            self._startTime = _parseTime(start)
            if self._startTime is None:
                seiscomp.logging.error("Wrong 'begin' format '%s'" % start)
                return False
            seiscomp.logging.debug("Setting start to %s"
                                   % self._startTime.toString("%FT%TZ"))

            try:
                end = self.commandline().optionString("end")
            except RuntimeError:
                pass

            self._endTime = _parseTime(end)
            if self._endTime is None:
                seiscomp.logging.error("Wrong 'end' format '%s'" % end)
                return False
            seiscomp.logging.debug("Setting end to %s"
                                   % self._endTime.toString("%FT%TZ"))
        else:
            seiscomp.logging.debug("Time window set by hours option: ignoring "
                                   "all other time parameters")
            secs = self.hours*3600
            maxSecs = 596523 * 3600
            if secs > maxSecs:
                seiscomp.logging.error("Maximum hours exceeeded. Maximum is %i" % (maxSecs / 3600))
                return False

            self._startTime = seiscomp.core.Time.UTC() - seiscomp.core.TimeSpan(secs)
            self._endTime = _parseTime(end)

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except RuntimeError:
            self._delimiter = "\n"

        try:
            modifiedAfter = self.commandline().optionString("modified-after")
            self._modifiedAfterTime = _parseTime(modifiedAfter)
            if self._modifiedAfterTime is None:
                seiscomp.logging.error("Wrong 'modified-after' format '%s'"
                                       % modifiedAfter)
                return False
            seiscomp.logging.debug(
                    "Setting 'modified-after' time to %s" %
                    self._modifiedAfterTime.toString("%FT%TZ"))
        except RuntimeError:
            pass

        try:
            self._preferredOrigin = self.commandline().hasOption("preferred-origin")
        except RuntimeError:
            pass

        return True

    def printUsage(self):

        print('''Usage:
  scevtls [options]

List event IDs available in a given time range and print to stdout.''')

        seiscomp.client.Application.printUsage(self)

        print('''Examples:
Print all event IDs from year 2022 and thereafter
  scevtls -d mysql://sysop:sysop@localhost/seiscomp --begin "2022-01-01 00:00:00"
''')

    def run(self):
        out = []
        seiscomp.logging.debug("Search interval: %s - %s" %
                               (self._startTime, self._endTime))
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

            outputString = evt.publicID()
            if self._preferredOrigin:
                try:
                    outputString += " " + evt.preferredOriginID()
                except ValueError:
                    outputString += " none"

            out.append(outputString)

        sys.stdout.write("%s\n" % self._delimiter.join(out))

        return True


def main():
    app = EventList(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
