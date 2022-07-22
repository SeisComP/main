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


class OriginList(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = seiscomp.core.Time()
        self._endTime = seiscomp.core.Time.GMT()
        self._delimiter = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Origins")
        self.commandline().addStringOption("Origins", "begin",
                                           "The lower bound of the time interval. Format: '1970-01-01 00:00:00'.")
        self.commandline().addStringOption("Origins", "end",
                                           "The upper bound of the time interval. Format: '1970-01-01 00:00:00'.")
        self.commandline().addStringOption("Origins", "author",
                                           "The author of the origins.")

        self.commandline().addGroup("Output")
        self.commandline().addStringOption("Output", "delimiter,D",
                                           "The delimiter of the resulting "
                                           "origin IDs. Default: '\\n')")
        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            start = self.commandline().optionString("begin")
            if not self._startTime.fromString(start, "%F %T"):
                print("Wrong 'begin' given -> assuming {}"
                      .format(self._startTime), file=sys.stderr)
        except RuntimeError:
            print("No 'begin' given -> assuming {}".format(self._startTime),
                  file=sys.stderr)

        try:
            end = self.commandline().optionString("end")
            if not self._endTime.fromString(end, "%F %T"):
                print("Wrong 'end' given -> assuming {}"
                      .format(self._endTime), file=sys.stderr)
        except RuntimeError:
            print("No 'end' given -> assuming {}".format(self._endTime),
                  file=sys.stderr)

        try:
            self.author = self.commandline().optionString("author")
            sys.stderr.write("%s author used for output\n" % (self.author))
        except RuntimeError:
            self.author = False

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except RuntimeError:
            self._delimiter = "\n"

#   sys.stderr.write("Setting end to %s\n" % self._endTime.toString("%F %T"))

        return True

    def printUsage(self):

        print('''Usage:
  scorgls [options]

List origin IDs available in a given time range and print to stdout.''')

        seiscomp.client.Application.printUsage(self)

        print('''Examples:
Print all origin IDs from year 2022 and thereafter
  scorgls -d mysql://sysop:sysop@localhost/seiscomp --begin "2022-01-01 00:00:00"
''')

    def run(self):
        seiscomp.logging.debug("Search interval: %s - %s" %
                               (self._startTime, self._endTime))
        out = []
        q = "select PublicObject.%s, Origin.* from Origin, PublicObject where Origin._oid=PublicObject._oid and Origin.%s >= '%s' and Origin.%s < '%s'" %\
            (self.database().convertColumnName("publicID"),
             self.database().convertColumnName("time_value"),
             self.database().timeToString(self._startTime),
             self.database().convertColumnName("time_value"),
             self.database().timeToString(self._endTime))

        if self.author:
            q += " and Origin.%s = '%s' " %\
                 (self.database().convertColumnName("creationInfo_author"),
                  self.query().toString(self.author))

        for obj in self.query().getObjectIterator(q, seiscomp.datamodel.Origin.TypeInfo()):
            org = seiscomp.datamodel.Origin.Cast(obj)
            if org:
                out.append(org.publicID())

        print("{}\n".format(self._delimiter.join(out)), file=sys.stdout)
        return True


def main():
    app = OriginList(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
