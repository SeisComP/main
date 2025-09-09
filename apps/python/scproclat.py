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

import time, sys, os, traceback
import seiscomp.core, seiscomp.client, seiscomp.datamodel
import seiscomp.logging, seiscomp.system


def createDirectory(dir):
    if os.access(dir, os.W_OK):
        return True

    try:
        os.makedirs(dir)
        return True
    except:
        return False


def timeToString(t):
    return t.toString("%T.%6f")


def timeSpanToString(ts):
    neg = ts.seconds() < 0 or ts.microseconds() < 0
    secs = abs(ts.seconds())
    days = secs / 86400
    daySecs = secs % 86400
    hours = daySecs / 3600
    hourSecs = daySecs % 3600
    mins = hourSecs / 60
    secs = hourSecs % 60
    usecs = abs(ts.microseconds())

    if neg:
        return "-%.2d:%.2d:%.2d:%.2d.%06d" % (days, hours, mins, secs, usecs)
    else:
        return "%.2d:%.2d:%.2d:%.2d.%06d" % (days, hours, mins, secs, usecs)


class ProcLatency(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(False, False)

        self.setAutoApplyNotifierEnabled(False)
        self.setInterpretNotifierEnabled(True)

        self.addMessagingSubscription("PICK")
        self.addMessagingSubscription("AMPLITUDE")
        self.addMessagingSubscription("LOCATION")
        self.addMessagingSubscription("MAGNITUDE")
        self.addMessagingSubscription("EVENT")

        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)

        self._directory = ""
        self._nowDirectory = ""
        self._triggeredDirectory = ""
        self._logCreated = False

    def createCommandLineDescription(self):
        try:
            self.commandline().addGroup("Storage")
            self.commandline().addStringOption(
                "Storage", "directory,o", "Specify the storage directory"
            )
        except:
            seiscomp.logging.warning(f"caught unexpected error {sys.exc_info()}")

    def initConfiguration(self):
        if not seiscomp.client.Application.initConfiguration(self):
            return False

        try:
            self._directory = self.configGetString("directory")
        except:
            pass

        try:
            self._logCreated = self.configGetBool("logMsgLatency")
        except:
            pass

        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            self._directory = self.commandline().optionString("directory")
        except:
            pass

        try:
            if self._directory[-1] != "/":
                self._directory = self._directory + "/"
        except:
            pass

        if self._directory:
            self._directory = seiscomp.system.Environment.Instance().absolutePath(
                self._directory
            )
            sys.stderr.write(f"Logging latencies to {self._directory}\n")

        return True

    def addObject(self, parentID, obj):
        try:
            self.logObject(parentID, obj, False)
        except:
            sys.stderr.write(f"{traceback.format_exc()}\n")

    def updateObject(self, parentID, obj):
        try:
            self.logObject("", obj, True)
        except:
            sys.stderr.write(f"{traceback.format_exc()}\n")

    def logObject(self, parentID, obj, update):
        now = seiscomp.core.Time.GMT()
        time = None

        pick = seiscomp.datamodel.Pick.Cast(obj)
        if pick:
            phase = ""
            try:
                phase = pick.phaseHint().code()
            except:
                pass

            created = None
            if self._logCreated:
                try:
                    created = pick.creationInfo().creationTime()
                except:
                    pass

            self.logStation(
                now,
                created,
                pick.time().value(),
                pick.publicID() + ";P;" + phase,
                pick.waveformID(),
                update,
            )
            return

        amp = seiscomp.datamodel.Amplitude.Cast(obj)
        if amp:
            created = None
            if self._logCreated:
                try:
                    created = amp.creationInfo().creationTime()
                except:
                    pass

            try:
                self.logStation(
                    now,
                    created,
                    amp.timeWindow().reference(),
                    amp.publicID()
                    + ";A;"
                    + amp.type()
                    + ";"
                    + f"{amp.amplitude().value():.2f}",
                    amp.waveformID(),
                    update,
                )
            except:
                pass
            return

        org = seiscomp.datamodel.Origin.Cast(obj)
        if org:
            status = ""
            lat = f"{org.latitude().value():.2f}"
            lon = f"{org.longitude().value():.2f}"
            try:
                depth = "%d" % org.depth().value()
            except:
                pass

            try:
                status = seiscomp.datamodel.EOriginStatusNames.name(org.status())
            except:
                pass

            self.logFile(
                now,
                org.time().value(),
                org.publicID() + ";O;" + status + ";" + lat + ";" + lon + ";" + depth,
                update,
            )
            return

        mag = seiscomp.datamodel.Magnitude.Cast(obj)
        if mag:
            count = ""
            try:
                count = "%d" % mag.stationCount()
            except:
                pass
            self.logFile(
                now,
                None,
                mag.publicID()
                + ";M;"
                + mag.type()
                + ";"
                + f"{mag.magnitude().value():.4f}"
                + ";"
                + count,
                update,
            )
            return

        orgref = seiscomp.datamodel.OriginReference.Cast(obj)
        if orgref:
            self.logFile(now, None, parentID + ";OR;" + orgref.originID(), update)
            return

        evt = seiscomp.datamodel.Event.Cast(obj)
        if evt:
            self.logFile(
                now,
                None,
                evt.publicID()
                + ";E;"
                + evt.preferredOriginID()
                + ";"
                + evt.preferredMagnitudeID(),
                update,
            )
            return

    def logStation(self, received, created, triggered, text, waveformID, update):
        streamID = (
            waveformID.networkCode()
            + "."
            + waveformID.stationCode()
            + "."
            + waveformID.locationCode()
            + "."
            + waveformID.channelCode()
        )

        aNow = received.get()
        aTriggered = triggered.get()

        nowDirectory = self._directory + "/".join(["%.2d" % i for i in aNow[1:4]]) + "/"
        triggeredDirectory = (
            self._directory + "/".join(["%.2d" % i for i in aTriggered[1:4]]) + "/"
        )

        logEntry = timeSpanToString(received - triggered) + ";"
        if created is not None:
            logEntry = logEntry + timeSpanToString(received - created) + ";"
        else:
            logEntry = logEntry + ";"

        if update:
            logEntry = logEntry + "U"
        else:
            logEntry = logEntry + "A"

        logEntry = logEntry + ";" + text

        sys.stdout.write(f"{timeToString(received)};{logEntry}\n")

        if nowDirectory != self._nowDirectory:
            if createDirectory(nowDirectory) == False:
                seiscomp.logging.error(f"Unable to create directory {nowDirectory}")
                return False

            self._nowDirectory = nowDirectory

        self.writeLog(
            self._nowDirectory + streamID + ".rcv",
            timeToString(received) + ";" + logEntry,
        )

        if triggeredDirectory != self._triggeredDirectory:
            if createDirectory(triggeredDirectory) == False:
                seiscomp.logging.error(
                    f"Unable to create directory {triggeredDirectory}"
                )
                return False

            self._triggeredDirectory = triggeredDirectory

        self.writeLog(
            self._triggeredDirectory + streamID + ".trg",
            timeToString(triggered) + ";" + logEntry,
        )

        return True

    def logFile(self, received, triggered, text, update):
        aNow = received.get()
        nowDirectory = self._directory + "/".join(["%.2d" % i for i in aNow[1:4]]) + "/"
        triggeredDirectory = None

        # logEntry = timeToString(received)
        logEntry = ""

        if not triggered is None:
            aTriggered = triggered.get()
            triggeredDirectory = (
                self._directory + "/".join(["%.2d" % i for i in aTriggered[1:4]]) + "/"
            )

            logEntry = logEntry + timeSpanToString(received - triggered)

        logEntry = logEntry + ";"

        if update:
            logEntry = logEntry + "U"
        else:
            logEntry = logEntry + "A"

        logEntry = logEntry + ";" + text

        sys.stdout.write(f"{timeToString(received)};{logEntry}\n")

        if nowDirectory != self._nowDirectory:
            if createDirectory(nowDirectory) == False:
                seiscomp.logging.error(f"Unable to create directory {nowDirectory}")
                return False

            self._nowDirectory = nowDirectory

        self.writeLog(
            self._nowDirectory + "objects.rcv", timeToString(received) + ";" + logEntry
        )

        if triggeredDirectory:
            if triggeredDirectory != self._triggeredDirectory:
                if createDirectory(triggeredDirectory) == False:
                    seiscomp.logging.error(
                        f"Unable to create directory {triggeredDirectory}"
                    )
                    return False

                self._triggeredDirectory = triggeredDirectory

            self.writeLog(
                self._triggeredDirectory + "objects.trg",
                timeToString(triggered) + ";" + logEntry,
            )

        return True

    def writeLog(self, file, text):
        of = open(file, "a")
        if of:
            of.write(text)
            of.write("\n")
            of.close()


app = ProcLatency(len(sys.argv), sys.argv)
sys.exit(app())
