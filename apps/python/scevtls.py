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

from seiscomp import core, client, datamodel, io, logging


class EventList(client.Application):
    def __init__(self, argc, argv):
        client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, False)
        self.setDaemonEnabled(False)

        self._startTime = None
        self._endTime = None
        self._delimiter = None
        self._modifiedAfterTime = None
        self._originCount = False
        self._preferredOrigin = False
        self._inputFile = None
        self._eventType = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Name of input XML file. Read from stdin if '-' is given. Deactivates "
            "reading of events from database",
        )
        self.commandline().addGroup("Events")
        self.commandline().addStringOption(
            "Events",
            "begin",
            "Specify the lower bound of the preferred origin time interval.",
        )
        self.commandline().addStringOption(
            "Events",
            "end",
            "Specify the upper bound of the preferred origin time interval.",
        )
        self.commandline().addStringOption(
            "Events",
            "hours",
            "Start searching given hours before now. If set, '--begin' and '--end' are "
            "ignored.",
        )
        self.commandline().addStringOption(
            "Events",
            "modified-after",
            "Filter for events modified after the specified time. Only the event "
            "modification time but not the modification time of related objects is "
            "considered.",
        )

        self.commandline().addStringOption(
            "Events",
            "event-type",
            "Select events whith specified event type. Use quotes for types with more "
            "one word. Use 'unknown' or an empty string to search for events having no "
            "type associated yet.",
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
        self.commandline().addOption(
            "Output",
            "origin-count,c",
            "Print the number of origins referenced by an event",
        )
        return True

    def validateParameters(self):
        if not client.Application.validateParameters(self):
            return False

        try:
            self._inputFile = self.commandline().optionString("input")
        except RuntimeError:
            pass

        if self._inputFile:
            self.setDatabaseEnabled(False, False)

        return True

    def init(self):
        def readTimeOption(option):
            try:
                value = self.commandline().optionString(option)
            except RuntimeError:
                return None

            try:
                time = core.Time.FromString(value)
                logging.debug(f"Setting {option} to {time.toString('%FT%TZ')}")
            except RuntimeError as e:
                logging.error(f"Wrong '{option}' format '{value}'")
                raise e

            return time

        if not client.Application.init(self):
            return False

        try:
            hours = float(self.commandline().optionString("hours"))
            self._startTime = core.Time.UTC() - core.TimeSpan(hours * 3600)
            logging.debug(
                "Time window start time set by hours option to "
                f"{self._startTime.iso()}, all other time parameters are ignored"
            )
        except ValueError as e:
            logging.error(f"Invalid 'hour' value: {e}")
            return False
        except RuntimeError:
            try:
                self._startTime = readTimeOption("begin")
            except RuntimeError:
                return False

            try:
                self._endTime = readTimeOption("end")
            except RuntimeError:
                return False

        try:
            self._delimiter = self.commandline().optionString("delimiter")
        except RuntimeError:
            self._delimiter = "\n"

        try:
            self._modifiedAfterTime = readTimeOption("modified-after")
        except RuntimeError:
            return False

        try:
            self._preferredOrigin = self.commandline().hasOption("preferred-origin")
        except RuntimeError:
            pass

        try:
            self._originCount = self.commandline().hasOption("origin-count")
        except RuntimeError:
            pass

        try:
            eventType = self.commandline().optionString("event-type").strip().lower()
            if eventType in ("unknown", ""):
                self._eventType = ""
            else:
                for i in range(datamodel.EEventTypeQuantity):
                    if eventType == datamodel.EEventTypeNames.name(i):
                        self._eventType = eventType
                        break

                if not self._eventType:
                    logging.error(f"'{eventType}' is not a valid SeisComP event type")
                    return False
        except RuntimeError:
            pass

        return True

    def printUsage(self):
        print(f"""Usage:
  {os.path.basename(__file__)} [options]

List event IDs available in a given time range and print to stdout.""")

        client.Application.printUsage(self)

        print(f"""Examples:
Print all event IDs from year 2022 and thereafter
  {os.path.basename(__file__)} -d mysql://sysop:sysop@localhost/seiscomp \
--begin 2022-01-01T00:00:00

Print all event IDs with event type 'quarry blast'
  {os.path.basename(__file__)} -d mysql://sysop:sysop@localhost/seiscomp --event-type 'quarry blast'

Print IDs of all events in XML file
  {os.path.basename(__file__)} -i events.xml
""")

    def readEventParameters(self):
        ar = io.XMLArchive()
        if not ar.open(self._inputFile):
            raise ValueError(f"Unable to open input file {self._inputFile}")

        obj = ar.readObject()
        if obj is None:
            raise TypeError("Invalid input file format")

        ep = datamodel.EventParameters.Cast(obj)
        if ep is None:
            raise ValueError("No event parameters found in input file")

        return ep

    def processEventParameters(self, ep):
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

            if self._eventType is not None:
                try:
                    eventType = datamodel.EEventTypeNames.name(evt.type())
                    if eventType != self._eventType:
                        continue
                except ValueError:
                    # empty string is used for unset event type
                    if self._eventType:
                        continue

            prefOrgID = evt.preferredOriginID()

            # filter by origin time
            org = ep.findOrigin(prefOrgID)
            orgTime = org.time().value()
            if self._startTime is not None and orgTime < self._startTime:
                continue
            if self._endTime is not None and orgTime > self._endTime:
                continue

            row = evt.publicID()
            if self._preferredOrigin:
                try:
                    row += f" {evt.preferredOriginID()}"
                except ValueError:
                    row += " none"

            if self._originCount:
                try:
                    row += f" {evt.originReferenceCount()}"
                except ValueError:
                    row += " 0"

            print(row, end=self._delimiter)

    def queryDB(self):
        def addWhere(whereClause, filterStr):
            if whereClause:
                return f" AND {filterStr}"

            return f" WHERE {filterStr}"

        def _T(name):
            return db.convertColumnName(name)

        def _time(time):
            return db.timeToString(time)

        db = self.database()

        # where clause filtering result by time, modification time or event type
        w = ""
        if self._startTime is not None:
            w += addWhere(w, f"o.{_T('time_value')} >= '{_time(self._startTime)}'")
        if self._endTime is not None:
            w += addWhere(w, f"o.{_T('time_value')} <= '{_time(self._endTime)}'")
        if self._modifiedAfterTime is not None:
            w += addWhere(
                w,
                (
                    f"e.{_T('creationInfo_modificationTime')} >= "
                    f"'{_time(self._modifiedAfterTime)}'"
                ),
            )
        if self._eventType is not None:
            # empty string is used for unset event type
            if self._eventType:
                eventTypeFilter = f"= '{self._eventType}'"
            else:
                eventTypeFilter = "IS NULL"
            w += addWhere(w, f"e.{_T('type')} {eventTypeFilter}")

        # select statement
        selPrefOrigin = (
            f", e.{_T('preferredOriginID')}" if self._preferredOrigin else ""
        )
        # if start or end time is filtered we need to join the origin table
        joinOrigin = ""
        if self._startTime is not None or self._endTime is not None:
            joinOrigin = (
                f" JOIN PublicObject po ON po.{_T('publicID')} = "
                f"e.{_T('preferredOriginID')} JOIN Origin o on o._oid = po._oid"
            )
        # count origin references
        selOrefCount = ""
        joinOref = ""
        if self._originCount:
            selOrefCount = ", COUNT(oref._oid)"
            joinOref = " LEFT JOIN OriginReference oref ON oref._parent_oid = e._oid"
            w += f" GROUP BY pe.{_T('publicID')}{selPrefOrigin}"

        query = (
            f"SELECT pe.{_T('publicID')}{selPrefOrigin}{selOrefCount} "
            "FROM PublicObject pe "
            "JOIN Event e ON e._oid = pe._oid"
            f"{joinOrigin}"
            f"{joinOref}"
            f"{w}"
        )

        logging.debug(f"Selecting events: {query}")
        if not db.beginQuery(query):
            logging.error(f"Could not query events: {query}")
            return False

        cols = db.getRowFieldCount()
        while db.fetchRow():
            print(
                " ".join(
                    db.getRowFieldString(i) if db.getRowFieldString(i) else "-"
                    for i in range(cols)
                ),
                end=self._delimiter,
            )

        return True

    def run(self):
        def fmtInterval():
            if self._startTime is None and self._endTime is None:
                return "unlimited"

            if self._startTime is None:
                return f"until {self._endTime.iso()}"

            if self._endTime is None:
                return f"from {self._startTime.iso()}"

            return f"{self._startTime.iso()} - {self._endTime.iso()}"

        logging.debug(f"Search interval: {fmtInterval()}")

        if self._inputFile:
            ep = self.readEventParameters()
            self.processEventParameters(ep)
            return True

        return self.queryDB()


def main():
    app = EventList(len(sys.argv), sys.argv)
    return app()


if __name__ == "__main__":
    sys.exit(main())
