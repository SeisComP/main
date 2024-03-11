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
import seiscomp.logging


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

    eventIDs = []
    for i in range(ep.eventCount()):
        evt = ep.event(i)

        if self._modifiedAfterTime is not None:
            try:
                if evt.creationInfo().modificationTime() < self._modifiedAfterTime:
                    continue
            except ValueError:
                try:
                    if evt.creationInfo().creationTime() < self._modifiedAfterTime:
                        continue
                except ValueError:
                    continue

        if self._eventType:
            try:
                eventType = seiscomp.datamodel.EEventTypeNames_name(evt.type())
                if eventType != self._eventType:
                    continue
            except ValueError:
                continue

        prefOrgID = evt.preferredOriginID()

        # filter by origin time
        org = ep.findOrigin(prefOrgID)
        orgTime = org.time().value()
        if orgTime < self._startTime:
            continue
        if orgTime > self._endTime:
            continue

        outputString = evt.publicID()
        if self._preferredOrigin:
            try:
                outputString += " " + evt.preferredOriginID()
            except ValueError:
                outputString += " none"

        eventIDs.append(outputString)

    return eventIDs


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
        self._inputFile = None
        self._eventType = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Name of input XML file. Read from stdin if '-' is given. Deactivates "
            "reading events from database",
        )
        self.commandline().addGroup("Events")
        self.commandline().addStringOption(
            "Events", "begin", "Specify the lower bound of the time interval."
        )
        self.commandline().addStringOption(
            "Events", "end", "Specify the upper bound of the time interval."
        )
        self.commandline().addStringOption(
            "Events",
            "hours",
            "Start searching given hours before"
            " now. If set, --begin and --end "
            "are ignored.",
        )
        self.commandline().addStringOption(
            "Events",
            "modified-after",
            "Select events modified after the specified time.",
        )

        self.commandline().addStringOption(
            "Events",
            "event-type",
            "Select events whith specified " "event type.",
        )

        self.commandline().addGroup("Output")
        self.commandline().addStringOption(
            "Output",
            "delimiter,D",
            "Specify the delimiter of the resulting event IDs. Default: '\\n')",
        )
        self.commandline().addOption(
            "Output",
            "preferred-origin,p",
            "Print the ID of the preferred origin along with the event ID.",
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
            self.hours = float(self.commandline().optionString("hours"))
        except RuntimeError:
            pass

        end = "2500-01-01T00:00:00Z"
        if self.hours is None:
            try:
                start = self.commandline().optionString("begin")
            except RuntimeError:
                start = "1900-01-01T00:00:00Z"

            self._startTime = seiscomp.core.Time.FromString(start)
            if self._startTime is None:
                seiscomp.logging.error(f"Wrong 'begin' format '{start}'")
                return False
            seiscomp.logging.debug(
                f"Setting start to {self._startTime.toString('%FT%TZ')}"
            )

            try:
                end = self.commandline().optionString("end")
            except RuntimeError:
                pass

            self._endTime = seiscomp.core.Time.FromString(end)
            if self._endTime is None:
                seiscomp.logging.error(f"Wrong 'end' format '{end}'")
                return False
            seiscomp.logging.debug(f"Setting end to {self._endTime.toString('%FT%TZ')}")
        else:
            seiscomp.logging.debug(
                "Time window set by hours option: ignoring all other time parameters"
            )
            secs = self.hours * 3600
            maxSecs = 596523 * 3600
            if secs > maxSecs:
                seiscomp.logging.error(
                    f"Maximum hours exceeeded. Maximum is {int(maxSecs / 3600)}"
                )
                return False

            self._startTime = seiscomp.core.Time.UTC() - seiscomp.core.TimeSpan(secs)
            self._endTime = seiscomp.core.Time.FromString(end)

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except RuntimeError:
            self._delimiter = "\n"

        try:
            modifiedAfter = self.commandline().optionString("modified-after")
            self._modifiedAfterTime = seiscomp.core.Time.FromString(modifiedAfter)
            if self._modifiedAfterTime is None:
                seiscomp.logging.error(
                    f"Wrong 'modified-after' format '{modifiedAfter}'"
                )
                return False
            seiscomp.logging.debug(
                f"Setting 'modified-after' time to {self._modifiedAfterTime.toString('%FT%TZ')}"
            )
        except RuntimeError:
            pass

        try:
            self._preferredOrigin = self.commandline().hasOption("preferred-origin")
        except RuntimeError:
            pass

        try:
            self._eventType = self.commandline().optionString("event-type")
        except RuntimeError:
            pass

        if self._eventType:
            flagEvent = False
            for i in range(seiscomp.datamodel.EEventTypeQuantity):
                if self._eventType == seiscomp.datamodel.EEventTypeNames.name(i):
                    flagEvent = True
                    break

            if not flagEvent:
                seiscomp.logging.error(
                    f"'{self._eventType}' is not a valid SeisComP event type"
                )
                return False

        return True

    def printUsage(self):
        print(
            f"""Usage:
  {os.path.basename(__file__)} [options]

List event IDs available in a given time range and print to stdout."""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            f"""Examples:
Print all event IDs from year 2022 and thereafter
  {os.path.basename(__file__)} -d mysql://sysop:sysop@localhost/seiscomp \
--begin "2022-01-01 00:00:00"

Print all event IDs with event type 'quarry blast'
  {os.path.basename(__file__)} -d mysql://sysop:sysop@localhost/seiscomp --event-type 'quarry blast'

Print IDs of all events in XML file
  {os.path.basename(__file__)} -i events.xml
"""
        )

    def run(self):
        out = []
        seiscomp.logging.debug(f"Search interval: {self._startTime} - {self._endTime}")

        if self._inputFile:
            out = readXML(self)
            sys.stdout.write(f"{self._delimiter.join(out)}\n")
            return True

        for obj in self.query().getEvents(self._startTime, self._endTime):
            evt = seiscomp.datamodel.Event.Cast(obj)
            if not evt:
                continue

            if self._modifiedAfterTime is not None:
                try:
                    if evt.creationInfo().modificationTime() < self._modifiedAfterTime:
                        continue
                except ValueError:
                    try:
                        if evt.creationInfo().creationTime() < self._modifiedAfterTime:
                            continue
                    except ValueError:
                        continue

            if self._eventType:
                try:
                    eventType = seiscomp.datamodel.EEventTypeNames_name(evt.type())
                    if eventType != self._eventType:
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

        sys.stdout.write(f"{self._delimiter.join(out)}\n")

        return True


def main():
    app = EventList(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
