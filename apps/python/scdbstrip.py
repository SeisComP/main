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

from __future__ import absolute_import, division, print_function

import re
import sys
import traceback
import seiscomp.core
import seiscomp.client
import seiscomp.logging
import seiscomp.utils

output = sys.stdout
error = sys.stderr


class RuntimeException(Exception):
    def __init__(self, what):
        self.what = what

    def __str__(self):
        return str(self.what)


class ExitRequestException(RuntimeException):
    def __init__(self):
        pass

    def __str__(self):
        return "exit requested"


class QueryInterface:
    def __init__(self, database):
        self._database = database

    def cnvCol(self, col):
        return self._database.convertColumnName(col)

    def getTables(self):
        return []

    def deleteObjectQuery(self, *v):
        return ""

    def deleteJournalQuery(self, *v):
        return ""

    def childQuery(self, mode, *v):
        return ""

    def childJournalQuery(self, mode, *v):
        return ""


class MySQLDB(QueryInterface):
    def __init__(self, database):
        QueryInterface.__init__(self, database)

    def getTables(self):
        tmp_tables = []
        if not self._database.beginQuery("show tables"):
            return tmp_tables

        while self._database.fetchRow():
            tmp_tables.append(self._database.getRowFieldString(0))

        self._database.endQuery()
        return tmp_tables

    def deleteObjectQuery(self, *v):
        if v[0]:
            q = "delete " + v[0] + " from " + ", ".join(v) + " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            q = "delete " + v[1] + " from " + ", ".join(v[1:]) + " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._oid=" + v[i+1] + "._oid"

        return q

    def deleteJournalQuery(self, *v):
        q = "delete JournalEntry from JournalEntry, " + ", ".join(v) + " where " + \
            v[0] + "._oid=" + v[1] + "._oid"

        for i in range(1, len(v)-1):
            q += " and " + v[i] + "._oid=" + v[i+1] + "._oid"

        q += " and JournalEntry.objectID=PublicObject.publicID"

        return q

    def childQuery(self, mode, *v):
        if v[0]:
            if mode == "delete":
                q = "delete " + v[0]
            elif mode == "count":
                q = "select count(*)"
            else:
                return ""

            q += " from " + ", ".join(v) + " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            if mode == "delete":
                q = "delete " + v[1]
            elif mode == "count":
                q = "select count(*)"
            else:
                return ""

            q += " from " + ", ".join(v[1:]) + " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._parent_oid=" + v[i+1] + "._oid"

        return q

    def childJournalQuery(self, mode, *v):
        if v[0]:
            if mode == "delete":
                q = "delete JournalEntry"
            elif mode == "count":
                q = "select count(*)"
            else:
                return ""

            q += " from JournalEntry, " + ", ".join(v) + " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            if mode == "delete":
                q = "delete " + v[1]
            elif mode == "count":
                q = "select count(*)"
            else:
                return ""

            q += " from JournalEntry, " + ", ".join(v[1:]) + " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._parent_oid=" + v[i+1] + "._oid"

        q += " and JournalEntry.objectID=PublicObject.publicID"
        return q


class PostgresDB(QueryInterface):
    def __init__(self, database):
        QueryInterface.__init__(self, database)

    def getTables(self):
        tmp_tables = []
        if not self._database.beginQuery("SELECT table_name FROM information_schema.tables WHERE table_type = 'BASE TABLE' AND table_schema NOT IN ('pg_catalog', 'information_schema');"):
            return tmp_tables

        while self._database.fetchRow():
            tmp_tables.append(self._database.getRowFieldString(0))

        self._database.endQuery()
        return tmp_tables

    def deleteObjectQuery(self, *v):
        if v[0]:
            q = "delete from " + v[0] + " using " + ", ".join(v[1:]) + " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            q = "delete from " + v[1] + " using " + \
                ", ".join(v[2:]) + " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._oid=" + v[i+1] + "._oid"

        return q

    def deleteJournalQuery(self, *v):
        q = "delete from JournalEntry using " + ", ".join(v) + " where " + \
            v[0] + "._oid=" + v[1] + "._oid"

        for i in range(1, len(v)-1):
            q += " and " + v[i] + "._oid=" + v[i+1] + "._oid"

        q += " and JournalEntry." + \
            self.cnvCol("objectID") + "=PublicObject." + \
            self.cnvCol("publicID")

        return q

    def childQuery(self, mode, *v):
        if v[0]:
            if mode == "delete":
                q = "delete from " + v[0] + " using " + ", ".join(v[1:])
            elif mode == "count":
                q = "select count(*) from " + ", ".join(v)
            else:
                return ""

            q += " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            if mode == "delete":
                q = "delete from " + v[1] + " using " + ", ".join(v[2:])
            elif mode == "count":
                q = "select count(*) from " + ", ".join(v[1:])
            else:
                return ""

            q += " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._parent_oid=" + v[i+1] + "._oid"

        return q

    def childJournalQuery(self, mode, *v):
        if v[0]:
            if mode == "delete":
                q = "delete from JournalEntry using "
            elif mode == "count":
                q = "select count(*) from "
            else:
                return ""

            q += ", ".join(v) + " where " + \
                v[0] + "._oid=" + v[1] + "._oid and "
        else:
            if mode == "delete":
                q = "delete from " + v[1] + " using "
            elif mode == "count":
                q = "select count(*) from "
            else:
                return ""

            q += " JournalEntry, " + ", ".join(v[1:]) + " where "

        for i in range(1, len(v)-1):
            if i > 1:
                q += " and "
            q += v[i] + "._parent_oid=" + v[i+1] + "._oid"

        q += " and JournalEntry." + \
            self.cnvCol("objectID") + "=PublicObject." + \
            self.cnvCol("publicID")
        return q


class DBCleaner(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, True)
        self.setDaemonEnabled(False)

        self._daysToKeep = None
        self._hoursToKeep = None
        self._minutesToKeep = None
        self._datetime = None
        self._invertMode = False
        self._stripEP = True
        self._stripQC = True

        self._steps = 0
        self._currentStep = 0
        self._keepEvents = []

        self._timer = seiscomp.utils.StopWatch()

    def createCommandLineDescription(self):
        try:
            try:
                self.commandline().addGroup("Mode")
                self.commandline().addOption("Mode", "check", "Checks if "
                                             "unreachable objects exist.")
                self.commandline().addOption("Mode", "clean-unused",
                                             "Remove all unreachable objects "
                                             "when in checkmode. Default: off.")

                self.commandline().addGroup("Objects")

                self.commandline().addOption("Objects", "ep-only,E",
                                             "Strip only event parameters"
                                             " but no waveform QC.")
                self.commandline().addStringOption("Objects", "keep-events",
                                                   "Event-IDs to keep in the "
                                                   "database. Combining with"
                                                   "'qc-only' is invalld.")
                self.commandline().addOption("Objects", "qc-only,Q",
                                             "Strip only waveform QC but no "
                                             "event parameters. Combining with"
                                             "'ep-only' is invalld.")

                self.commandline().addGroup("Timespan")
                self.commandline().addStringOption("Timespan", "datetime",
                                                   "Specify the datetime (UTC)"
                                                   " from which to keep all "
                                                   "events. If given, days, "
                                                   "minutes and hours are ignored. "
                                                   "Format: '%Y-%m-%d %H:%M:%S'.")
                self.commandline().addIntOption("Timespan", "days",
                                                "The number of days to keep. "
                                                "Added to hours and minutes. "
                                                "Default is 30 if no other "
                                                "times are given.")
                self.commandline().addIntOption("Timespan", "hours",
                                                "The number of hours to keep. "
                                                "Added to days and minutes.")
                self.commandline().addIntOption("Timespan", "minutes",
                                                "The number of minutes to keep. "
                                                "Added to days and hours.")
                self.commandline().addOption("Timespan", "invert,i",
                                             "Delete all parameters after the "
                                             "specified time period. If not "
                                             "given, parameter from before are"
                                             " deleted.")

            except RuntimeError:
                seiscomp.logging.warning(
                    "caught unexpected error %s" % sys.exc_info())
            return True
        except RuntimeError:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

    def initConfiguration(self):
        try:
            if not seiscomp.client.Application.initConfiguration(self):
                return False
            try:
                self._invertMode = self.configGetBool(
                    "database.cleanup.invertMode")
            except RuntimeError:
                pass

            try:
                if self.configGetBool("database.cleanup.eventParameters"):
                    self._stripEP = True
                else:
                    self._stripEP = False
            except RuntimeError:
                pass

            try:
                if self.configGetBool("database.cleanup.qualityControl"):
                    self._stripQC = True
                else:
                    self._stripQC = False
            except RuntimeError:
                pass

            try:
                self._daysToKeep = self.configGetInt(
                    "database.cleanup.keep.days")
            except RuntimeError:
                pass

            try:
                self._hoursToKeep = self.configGetInt(
                    "database.cleanup.keep.hours")
            except RuntimeError:
                pass

            try:
                self._minutesToKeep = self.configGetInt(
                    "database.cleanup.keep.minutes")
            except RuntimeError:
                pass

            return True

        except RuntimeError:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)


    def printUsage(self):

        print('''Usage:
  scbstrip [options]

Remove event and waveform quality parameters from the database in a timespan.''')

        seiscomp.client.Application.printUsage(self)

        print('''Examples:
Remove all event and waveform QC paramters older than 30 days
  scdbstrip -d mysql://sysop:sysop@localhost/seiscomp --days 30
''')

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        try:
            try:
                self._daysToKeep = self.commandline().optionInt("days")
            except RuntimeError:
                pass

            try:
                self._hoursToKeep = self.commandline().optionInt("hours")
            except RuntimeError:
                pass

            try:
                self._minutesToKeep = self.commandline().optionInt("minutes")
            except RuntimeError:
                pass

            if self.commandline().hasOption("invert"):
                self._invertMode = True

            epOnly = False
            if self.commandline().hasOption("ep-only"):
                self._stripEP = True
                self._stripQC = False
                epOnly = True

            if self.commandline().hasOption("qc-only"):
                if epOnly:
                    error.write("ERROR: Option '--qc-only' conflicts with "
                                "'--ep-only'\n")
                    return False
                else:
                    self._stripEP = False
                    self._stripQC = True

            if not self._stripEP and not self._stripQC:
                error.write("[INFO] Event and QC parameters are disregarded by"
                            " configuration\n")
                return False
            try:
                eventIDs = self.commandline().optionString("keep-events")
                self._keepEvents = [id.strip() for id in eventIDs.split(',')]
            except RuntimeError:
                pass

            try:
                dateTime = self.commandline().optionString("datetime")
            except RuntimeError:
                dateTime = None

            if dateTime:
                self._daysToKeep = None
                self._hoursToKeep = None
                self._minutesToKeep = None

                date = seiscomp.core.Time()
                try:
                    if date.fromString(dateTime, "%Y-%m-%d %H:%M:%S"):
                        error.write("Using datetime option: %s\n" %
                                    date.toString("%Y-%m-%d %H:%M:%S"))
                        self._datetime = date
                    else:
                        error.write("ERROR: datetime has wrong format\n")
                        return False
                except ValueError:
                    pass

            # fall back to default if no times are given
            if (self._daysToKeep is None and dateTime is None and
                    self._hoursToKeep is None and self._minutesToKeep is None):
                self._daysToKeep = 30

            return True

        except RuntimeError:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

    def run(self):
        classname = self.database().className()
        if re.search('postgres', classname, re.IGNORECASE):
            self._query = PostgresDB(self.database())
        elif re.search('mysql', classname, re.IGNORECASE):
            self._query = MySQLDB(self.database())
        else:
            output.write(
                "Error: Database interface %s is not supported\n" % (classname))
            output.flush()
            return False

        try:
            self._timer.restart()

            if self.commandline().hasOption("check"):
                return self.check()

            return self.clean()
        except RuntimeError:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

    def checkTable(self, table):
        self.runCommand(
            "update tmp_object set used=1 where _oid in (select _oid from %s)"
            % table)

    def check(self):
        try:
            if self._datetime is None:
                timeSpan = seiscomp.core.TimeSpan(0)
                if self._daysToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._daysToKeep*24*3600)

                if self._hoursToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._hoursToKeep*3600)

                if self._minutesToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._minutesToKeep*60)

                # All times are given in localtime
                timestamp = seiscomp.core.Time.LocalTime() - timeSpan
            else:
                timestamp = self._datetime

            output.write("[INFO] Check objects older than %s\n" %
                         timestamp.toString("%Y-%m-%d %H:%M:%S"))

            tables = self._query.getTables()
            if len(tables) == 0:
                return False

            if "Object" in tables:
                tables.remove("Object")
            if "object" in tables:
                tables.remove("object")
            if "PublicObject" in tables:
                tables.remove("PublicObject")
            if "publicobject" in tables:
                tables.remove("publicobject")
            if "Meta" in tables:
                tables.remove("Meta")
            if "meta" in tables:
                tables.remove("meta")

            self._steps = len(tables) + 1

            if self.commandline().hasOption("clean-unused"):
                self._steps = self._steps + 1

            # Skip the first 5 objects id' that are reserved for metaobjects
            #  (Config, QualityControl, inventory, EventParameters, routing)
            tmp_object = "\
      create temporary table tmp_object as \
        select _oid, 0 as used from Object where _oid > 5 and _timestamp < '%s'\
      " % timestamp.toString("%Y-%m-%d %H:%M:%S")

            self.beginMessage("Search objects")
            if not self.runCommand(tmp_object):
                return False
            self.endMessage(self.globalCount("tmp_object"))

            for table in tables:
                self.beginMessage("Check table %s" % table)
                self.checkTable(table)
                self.endMessage(self.usedCount("tmp_object"))

            unusedObjects = self.unusedCount("tmp_object")

            if self.commandline().hasOption("clean-unused"):
                self.delete("Remove unreachable objects",
                            self.deleteUnusedRawObjects, "tmp_object")

            self.beginMessage("%d unused objects found" % unusedObjects)
            if not self.runCommand("drop table tmp_object"):
                return False
            self.endMessage()

            return True

        except RuntimeException as e:
            error.write("\nException: %s\n" % str(e))
            return False

        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

    def clean(self):
        try:
            if self._datetime is None:
                timeSpan = seiscomp.core.TimeSpan(0)
                if self._daysToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._daysToKeep*24*3600)

                if self._hoursToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._hoursToKeep*3600)

                if self._minutesToKeep:
                    timeSpan += seiscomp.core.TimeSpan(self._minutesToKeep*60)

                # All times are given in GMT (UTC)
                timestamp = seiscomp.core.Time.GMT() - timeSpan
            else:
                timestamp = self._datetime

            if not self._invertMode:
                output.write("[INFO] Keep objects after %s UTC\n" %
                             timestamp.toString("%Y-%m-%d %H:%M:%S"))
            else:
                output.write("[INFO] Keep objects before %s UTC\n" %
                             timestamp.toString("%Y-%m-%d %H:%M:%S"))

            if len(self._keepEvents) > 0:
                output.write("[INFO] Keep events in db: %s\n" %
                             ",".join(self._keepEvents))

            op = '<'
            if self._invertMode:
                op = '>='

            self._steps = 32

            # treat QC entries
            if self._stripQC:
                self.beginMessage("Deleting waveform quality parameters")
                if not self.runCommand(
                        self._query.deleteObjectQuery("Object", "WaveformQuality")
                        + "WaveformQuality.%s %s '%s'" %
                        (self.cnvCol("end"), op,
                         timestamp.toString("%Y-%m-%d %H:%M:%S"))):
                    return False
                if not self.runCommand("delete from WaveformQuality where WaveformQuality.%s %s '%s'" % (self.cnvCol("end"), op, timestamp.toString("%Y-%m-%d %H:%M:%S"))):
                    return False
                self.endMessage()

            if not self._stripEP:
                return True

            # treat event parameters
            old_events = "\
      create temporary table old_events as \
        select Event._oid, PEvent.%s \
        from Event, PublicObject as PEvent, Origin, PublicObject as POrigin \
        where Event._oid=PEvent._oid and \
              Origin._oid=POrigin._oid and \
              Event.%s=POrigin.%s and \
              Origin.%s %s '%s'\
      " % (self.cnvCol("publicID"), self.cnvCol("preferredOriginID"), self.cnvCol("publicID"), self.cnvCol("time_value"), op, timestamp.toString("%Y-%m-%d %H:%M:%S"))

            if len(self._keepEvents) > 0:
                old_events += " and PEvent." + \
                    self.cnvCol("publicID") + \
                    " not in ('%s')" % "','".join(self._keepEvents)

            self.beginMessage("Find old events")
            if not self.runCommand(old_events):
                return False
            self.endMessage(self.globalCount("old_events"))

            # Delete OriginReferences of old events
            self.delete("Delete origin references of old events",
                        self.deleteChilds, "OriginReference", "old_events")

            # Delete FocalMechanismReference of old events
            self.delete("Delete focal mechanism references of old events",
                        self.deleteChilds, "FocalMechanismReference",
                        "old_events")

            # Delete EventDescription of old events
            self.delete("Delete event descriptions of old events",
                        self.deleteChilds, "EventDescription", "old_events")

            # Delete Comments of old events
            self.delete("Delete comments of old events",
                        self.deleteChilds, "Comment", "old_events")

            # Delete old events
            self.delete("Delete old events", self.deleteObjects,
                        "Event", "old_events")

            self.beginMessage("Cleaning up temporary results")
            if not self.runCommand("drop table old_events"):
                return False
            self.endMessage()

            tmp_fm = "\
      create temporary table tmp_fm as \
        select FocalMechanism._oid, PFM.%s, 0 as used \
        from PublicObject as PFM, FocalMechanism \
        where PFM._oid=FocalMechanism._oid\
      " % (self.cnvCol("publicID"))

            self.beginMessage("Find unassociated focal mechanisms")

            if not self.runCommand(tmp_fm):
                return False

            tmp_fm = "\
      update tmp_fm set used=1 \
      where " + self.cnvCol("publicID") + " in (select distinct " + self.cnvCol("focalMechanismID") + " from FocalMechanismReference) \
      "

            if not self.runCommand(tmp_fm):
                return False

            self.endMessage(self.unusedCount("tmp_fm"))

            # Delete Comments of unassociated focal mechanisms
            self.delete("Delete comments of unassociation focal mechanisms",
                        self.deleteUnusedChilds, "Comment", "tmp_fm")

            # Delete MomentTensor.Comments of unassociated focal mechanisms
            self.delete("Delete moment tensor comments of unassociated focal mechanisms",
                        self.deleteUnusedChilds, "Comment", "MomentTensor",
                        "tmp_fm")

            # Delete MomentTensor.DataUsed of unassociated focal mechanisms
            self.delete("Delete moment tensor data of unassociated focal mechanisms",
                        self.deleteUnusedChilds, "DataUsed", "MomentTensor",
                        "tmp_fm")

            # Delete MomentTensor.PhaseSetting of unassociated focal mechanisms
            self.delete("Delete moment tensor phase settings of unassociated focal mechanisms",
                        self.deleteUnusedChilds, "MomentTensorPhaseSetting",
                        "MomentTensor", "tmp_fm")

            # Delete MomentTensor.StationContribution.ComponentContribution of unassociated focal mechanisms
            self.delete("Delete moment tensor component contributions of unassociated focal mechanisms",
                        self.deleteUnusedChilds,
                        "MomentTensorComponentContribution",
                        "MomentTensorStationContribution", "MomentTensor",
                        "tmp_fm")

            # Delete MomentTensor.StationContributions of unassociated focal mechanisms
            self.delete("Delete moment tensor station contributions of unassociated focal mechanisms",
                        self.deleteUnusedPublicChilds,
                        "MomentTensorStationContribution", "MomentTensor",
                        "tmp_fm")

            # Delete MomentTensors of unassociated focal mechanisms
            self.delete("Delete moment tensors of unassociated focal mechanisms",
                        self.deleteUnusedPublicChilds, "MomentTensor",
                        "tmp_fm")

            # Delete FocalMechanism itself
            self.delete("Delete unassociated focal mechanisms",
                        self.deleteUnusedObjects, "FocalMechanism", "tmp_fm")

            self.beginMessage("Cleaning up temporary results")
            if not self.runCommand("drop table tmp_fm"):
                return False
            self.endMessage()

            tmp_origin = "\
      create temporary table tmp_origin as \
        select Origin._oid, POrigin.%s, 0 as used \
        from PublicObject as POrigin, Origin \
        where POrigin._oid=Origin._oid and \
              Origin.%s %s '%s'\
      " % (self.cnvCol("publicID"), self.cnvCol("time_value"), op, timestamp.toString("%Y-%m-%d %H:%M:%S"))

            self.beginMessage("Find unassociated origins")

            if not self.runCommand(tmp_origin):
                return False

            tmp_origin = "\
      update tmp_origin set used=1 \
      where (" + self.cnvCol("publicID") + " in (select distinct " + self.cnvCol("originID") + " from OriginReference)) \
      or (" + self.cnvCol("publicID") + " in (select " + self.cnvCol("derivedOriginID") + " from MomentTensor))"

            if not self.runCommand(tmp_origin):
                return False

            self.endMessage(self.unusedCount("tmp_origin"))

            # Delete Arrivals of unassociated origins
            self.delete("Delete unassociated arrivals",
                        self.deleteUnusedChilds, "Arrival", "tmp_origin")

            # Delete StationMagnitudes of unassociated origins
            self.delete("Delete unassociated station magnitudes",
                        self.deleteUnusedPublicChilds, "StationMagnitude",
                        "tmp_origin")

            # Delete StationMagnitudeContributions of unassociated origins
            self.delete("Delete unassociated station magnitude contributions",
                        self.deleteUnusedChilds,
                        "StationMagnitudeContribution", "Magnitude",
                        "tmp_origin")

            # Delete Magnitudes of unassociated origins
            self.delete("Delete unassociated magnitudes",
                        self.deleteUnusedPublicChilds, "Magnitude",
                        "tmp_origin")

            # Delete Comments of unassociated origins
            self.delete("Delete comments of unassociation origins",
                        self.deleteUnusedChilds, "Comment", "tmp_origin")

            # Delete CompositeTimes of unassociated origins
            self.delete("Delete composite times of unassociation origins",
                        self.deleteUnusedChilds, "CompositeTime", "tmp_origin")

            # Delete Origins itself
            self.delete("Delete unassociated origins",
                        self.deleteUnusedObjects, "Origin", "tmp_origin")

            self.beginMessage("Cleaning up temporary results")
            if not self.runCommand("drop table tmp_origin"):
                return False
            self.endMessage()

            # Delete all unassociated picks (via arrivals)

            self.beginMessage("Find unassociated picks")

            tmp_pick = "\
      create temporary table tmp_pick as \
        select Pick._oid, PPick.%s, 0 as used \
        from PublicObject as PPick, Pick \
        where PPick._oid=Pick._oid and \
              Pick.%s %s '%s' \
      " % (self.cnvCol("publicID"), self.cnvCol("time_value"), op, timestamp.toString("%Y-%m-%d %H:%M:%S"))

            if not self.runCommand(tmp_pick):
                return False

            tmp_pick = "\
      update tmp_pick set used=1 \
      where " + self.cnvCol("publicID") + " in \
        (select distinct " + self.cnvCol("pickID") + " from Arrival) \
      "

            if not self.runCommand(tmp_pick):
                return False

            self.endMessage(self.unusedCount("tmp_pick"))

            self.delete("Delete unassociated picks",
                        self.deleteUnusedObjects, "Pick", "tmp_pick")

            self.beginMessage("Cleaning up temporary results")
            if not self.runCommand("drop table tmp_pick"):
                return False
            self.endMessage()

            # Delete all unassociated amplitudes (via stationmagnitudes)

            self.beginMessage("Find unassociated amplitudes")

            tmp_amp = "\
      create temporary table tmp_amp as \
        select Amplitude._oid, PAmplitude.%s, 0 as used \
        from PublicObject as PAmplitude, Amplitude \
        where PAmplitude._oid=Amplitude._oid and \
              Amplitude.%s %s '%s' \
      " % (self.cnvCol("publicID"), self.cnvCol("timeWindow_reference"), op, timestamp.toString("%Y-%m-%d %H:%M:%S"))

            if not self.runCommand(tmp_amp):
                return False

            tmp_amp = "\
      update tmp_amp set used=1 \
      where " + self.cnvCol("publicID") + " in \
        (select distinct " + self.cnvCol("amplitudeID") + " from StationMagnitude) \
      "

            if not self.runCommand(tmp_amp):
                return False

            self.endMessage(self.unusedCount("tmp_amp"))

            self.delete("Delete unassociated station amplitudes",
                        self.deleteUnusedObjects, "Amplitude", "tmp_amp")

            self.beginMessage("Cleaning up temporary results")
            if not self.runCommand("drop table tmp_amp"):
                return False
            self.endMessage()

            return True

        except RuntimeException as e:
            error.write("\nException: %s\n" % str(e))
            return False

        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

    def cnvCol(self, col):
        return self.database().convertColumnName(col)

    def beginMessage(self, msg):
        output.write("[%3d%%] " % (self._currentStep*100/self._steps))
        output.write(msg + "...")
        output.flush()
        self._currentStep = self._currentStep + 1

    def endMessage(self, count=None):
        if not count is None:
            output.write("done (%d)" % count)
        else:
            output.write("done")

        span = self._timer.elapsed().seconds()
        output.write(", time spent: %d %02d:%02d:%02d\n" % (
            span / 86400, (span % 86400) / 3600, (span % 3600) / 60, span % 60))

    def runCommand(self, q):
        if self.isExitRequested():
            raise ExitRequestException

        if not self.database().execute(q):
            raise RuntimeException("ERROR: command '%s' failed\n" % q)

        if self.isExitRequested():
            raise ExitRequestException

        return True

    def runQuery(self, q):
        if self.isExitRequested():
            raise ExitRequestException

        count = "-1"

        if not self.database().beginQuery(q):
            raise RuntimeException("ERROR: command '%s' failed\n" % q)

        if self.database().fetchRow():
            count = self.database().getRowFieldString(0)

        self.database().endQuery()

        if self.isExitRequested():
            raise ExitRequestException

        return [count]

    def globalCount(self, table):
        return int(self.runQuery("select count(*) from %s" % table)[0])

    def usedCount(self, table):
        return int(self.runQuery("select count(*) from %s where used=1" % table)[0])

    def unusedCount(self, table):
        return int(self.runQuery("select count(*) from %s where used=0" % table)[0])

    def deleteChilds(self, *v):
        count = int(self.runQuery(
            self._query.childQuery("count", "Object", *v))[0])
        self.runCommand(self._query.childQuery("delete", "Object", *v))
        self.runCommand(self._query.childQuery("delete", None, *v))
        return count

    def deleteUnusedChilds(self, *v):
        count = int(self.runQuery(self._query.childQuery(
            "count", "Object", *v) + " and used=0")[0])
        self.runCommand(self._query.childQuery(
            "delete", "Object", *v) + " and used=0")
        self.runCommand(self._query.childQuery(
            "delete", None, *v) + " and used=0")
        return count

    def deleteUnusedPublicChilds(self, *v):
        count = int(self.runQuery(self._query.childQuery(
            "count", "Object", *v) + " and used=0")[0])
        self.runCommand(self._query.childJournalQuery(
            "delete", "PublicObject", *v) + " and used=0")
        self.runCommand(self._query.childQuery(
            "delete", "Object", *v) + " and used=0")
        self.runCommand(self._query.childQuery(
            "delete", "PublicObject", *v) + " and used=0")
        self.runCommand(self._query.childQuery(
            "delete", None, *v) + " and used=0")
        return count

    def deleteUnusedRawObjects(self, *v):
        self.runCommand(self._query.deleteJournalQuery(
            "PublicObject", *v) + " and used=0")
        self.runCommand(self._query.deleteObjectQuery(
            None, "Object", *v) + " and used=0")
        self.runCommand(self._query.deleteObjectQuery(
            None, "PublicObject", *v) + " and used=0")
        return None

    def deleteObjects(self, *v):
        self.runCommand(self._query.deleteJournalQuery("PublicObject", *v))
        self.runCommand(self._query.deleteObjectQuery("Object", *v))
        self.runCommand(self._query.deleteObjectQuery("PublicObject", *v))
        self.runCommand(self._query.deleteObjectQuery(None, *v))
        return None

    def deleteUnusedObjects(self, *v):
        self.runCommand(self._query.deleteJournalQuery(
            "PublicObject", *v) + " and used=0")
        self.runCommand(self._query.deleteObjectQuery(
            "Object", *v) + " and used=0")
        self.runCommand(self._query.deleteObjectQuery(
            "PublicObject", *v) + " and used=0")
        self.runCommand(self._query.deleteObjectQuery(
            None, *v) + " and used=0")
        return None

    def delete(self, message, func, *v):
        self.beginMessage(message)
        count = func(*v)
        self.endMessage(count)
        return count


app = DBCleaner(len(sys.argv), sys.argv)
sys.exit(app())
