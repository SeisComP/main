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
import re
import subprocess
import traceback
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.math
import seiscomp.logging
import seiscomp.seismology
import seiscomp.system


class ObjectAlert(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setLoadRegionsEnabled(True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)
        self.addMessagingSubscription("EVENT")
        self.addMessagingSubscription("LOCATION")
        self.addMessagingSubscription("MAGNITUDE")

        self.setAutoApplyNotifierEnabled(True)
        self.setInterpretNotifierEnabled(True)

        self.setLoadCitiesEnabled(True)
        self.setLoadRegionsEnabled(True)

        self._ampType = "snr"
        self._citiesMaxDist = 20
        self._citiesMinPopulation = 50000

        self._eventDescriptionPattern = None
        self._pickScript = None
        self._ampScript = None
        self._alertScript = None
        self._eventScript = None

        self._pickProc = None
        self._ampProc = None
        self._alertProc = None
        self._eventProc = None

        self._newWhenFirstSeen = False
        self._oldEvents = []
        self._agencyIDs = []
        self._authors = []
        self._phaseHints = []
        self._phaseStreams = []
        self._phaseNumber = 1
        self._phaseInterval = 1
        self._cache = None
        self._pickCache = seiscomp.datamodel.PublicObjectTimeSpanBuffer()

    def createCommandLineDescription(self):
        self.commandline().addOption(
            "Generic",
            "first-new",
            "calls an event a new event when it is seen the first time",
        )
        self.commandline().addGroup("Alert")
        self.commandline().addStringOption(
            "Alert", "amp-type", "amplitude type to listen to", self._ampType
        )
        self.commandline().addStringOption(
            "Alert",
            "pick-script",
            "script to be called when a pick arrived, network-, station code pick "
            "publicID are passed as parameters $1, $2, $3 and $4",
        )
        self.commandline().addStringOption(
            "Alert",
            "amp-script",
            "script to be called when a station amplitude arrived, network-, station "
            "code, amplitude and amplitude publicID are passed as parameters $1, $2, $3 and $4",
        )
        self.commandline().addStringOption(
            "Alert",
            "alert-script",
            "script to be called when a preliminary origin arrived, latitude and "
            "longitude are passed as parameters $1 and $2",
        )
        self.commandline().addStringOption(
            "Alert",
            "event-script",
            "script to be called when an event has been declared; the message string, a "
            "flag (1=new event, 0=update event), the EventID, the arrival count and the "
            "magnitude (optional when set) are passed as parameter $1, $2, $3, $4 and $5",
        )
        self.commandline().addGroup("Cities")
        self.commandline().addStringOption(
            "Cities",
            "max-dist",
            "maximum distance for using the distance from a city to the earthquake",
        )
        self.commandline().addStringOption(
            "Cities",
            "min-population",
            "minimum population for a city to become a point of interest",
        )
        self.commandline().addGroup("Debug")
        self.commandline().addStringOption("Debug", "eventid,E", "specify Event ID")
        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        foundScript = False
        # module configuration paramters
        try:
            self._newWhenFirstSeen = self.configGetBool("firstNew")
        except RuntimeError:
            pass

        try:
            self._agencyIDs = [self.configGetString("agencyID")]
        except RuntimeError:
            pass

        try:
            agencyIDs = self.configGetStrings("agencyIDs")
            self._agencyIDs = []
            for item in agencyIDs:
                item = item.strip()
                if item and item not in self._agencyIDs:
                    self._agencyIDs.append(item)
        except RuntimeError:
            pass

        try:
            authors = self.configGetStrings("authors")
            self._authors = []
            for item in authors:
                item = item.strip()
                if item not in self._authors:
                    self._authors.append(item)
        except RuntimeError:
            pass

        self._phaseHints = ["P", "S"]
        try:
            phaseHints = self.configGetStrings("constraints.phaseHints")
            self._phaseHints = []
            for item in phaseHints:
                item = item.strip()
                if item not in self._phaseHints:
                    self._phaseHints.append(item)
        except RuntimeError:
            pass

        self._phaseStreams = []
        try:
            phaseStreams = self.configGetStrings("constraints.phaseStreams")
            for item in phaseStreams:
                rule = item.strip()
                # rule is NET.STA.LOC.CHA and the special charactes ? * | ( ) are allowed
                if not re.fullmatch(r"[A-Z|a-z|0-9|\?|\*|\||\(|\)|\.]+", rule):
                    seiscomp.logging.error(
                        f"Wrong stream ID format in `constraints.phaseStreams`: {item}"
                    )
                    return False
                # convert rule to a valid regular expression
                rule = re.sub(r"\.", r"\.", rule)
                rule = re.sub(r"\?", ".", rule)
                rule = re.sub(r"\*", ".*", rule)
                if rule not in self._phaseStreams:
                    self._phaseStreams.append(rule)
        except RuntimeError:
            pass

        try:
            self._phaseNumber = self.configGetInt("constraints.phaseNumber")
        except RuntimeError:
            pass

        try:
            self._phaseInterval = self.configGetInt("constraints.phaseInterval")
        except RuntimeError:
            pass

        if self._phaseNumber > 1:
            self._pickCache.setTimeSpan(seiscomp.core.TimeSpan(self._phaseInterval))
            self.enableTimer(1)

        try:
            self._eventDescriptionPattern = self.configGetString("poi.message")
        except RuntimeError:
            pass

        try:
            self._citiesMaxDist = self.configGetDouble("poi.maxDist")
        except RuntimeError:
            pass

        try:
            self._citiesMinPopulation = self.configGetInt("poi.minPopulation")
        except RuntimeError:
            pass

        # mostly command-line options
        try:
            self._citiesMaxDist = self.commandline().optionDouble("max-dist")
        except RuntimeError:
            pass

        try:
            if self.commandline().hasOption("first-new"):
                self._newWhenFirstSeen = True
        except RuntimeError:
            pass

        try:
            self._citiesMinPopulation = self.commandline().optionInt("min-population")
        except RuntimeError:
            pass

        try:
            self._ampType = self.commandline().optionString("amp-type")
        except RuntimeError:
            pass

        try:
            self._pickScript = self.commandline().optionString("pick-script")
        except RuntimeError:
            try:
                self._pickScript = self.configGetString("scripts.pick")
            except RuntimeError:
                seiscomp.logging.warning("No pick script defined")

        if self._pickScript:
            self._pickScript = seiscomp.system.Environment.Instance().absolutePath(
                self._pickScript
            )
            seiscomp.logging.info(f"Using pick script {self._pickScript}")

            if not os.path.isfile(self._pickScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._pickScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._ampScript = self.commandline().optionString("amp-script")
        except RuntimeError:
            try:
                self._ampScript = self.configGetString("scripts.amplitude")
            except RuntimeError:
                seiscomp.logging.warning("No amplitude script defined")

        if self._ampScript:
            self._ampScript = seiscomp.system.Environment.Instance().absolutePath(
                self._ampScript
            )
            seiscomp.logging.info(f"Using amplitude script {self._ampScript}")

            if not os.path.isfile(self._ampScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._ampScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._alertScript = self.commandline().optionString("alert-script")
        except RuntimeError:
            try:
                self._alertScript = self.configGetString("scripts.alert")
            except RuntimeError:
                seiscomp.logging.warning("No alert script defined")

        if self._alertScript:
            self._alertScript = seiscomp.system.Environment.Instance().absolutePath(
                self._alertScript
            )
            seiscomp.logging.info(f"Using alert script {self._alertScript}")

            if not os.path.isfile(self._alertScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._alertScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._eventScript = self.commandline().optionString("event-script")
        except RuntimeError:
            try:
                self._eventScript = self.configGetString("scripts.event")
            except RuntimeError:
                seiscomp.logging.warning("No event script defined")

        if self._eventScript:
            self._eventScript = seiscomp.system.Environment.Instance().absolutePath(
                self._eventScript
            )
            seiscomp.logging.info(f"Using event script {self._eventScript}")

            if not os.path.isfile(self._eventScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._eventScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        if not foundScript:
            seiscomp.logging.error("Found no valid script in configuration")
            return False

        seiscomp.logging.info("Creating ringbuffer for 100 objects")
        if not self.query():
            seiscomp.logging.warning("No valid database interface to read from")
        self._cache = seiscomp.datamodel.PublicObjectRingBuffer(self.query(), 100)

        if self._ampScript and self.connection():
            seiscomp.logging.info(
                "Amplitude script defined: subscribing to AMPLITUDE message group"
            )
            self.connection().subscribe("AMPLITUDE")

        if self._pickScript and self.connection():
            seiscomp.logging.info(
                "Pick script defined: subscribing to PICK message group"
            )
            self.connection().subscribe("PICK")

        if self._newWhenFirstSeen:
            seiscomp.logging.info(
                "A new event is declared when I see it the first time"
            )

        seiscomp.logging.info("Filtering:")
        if self._agencyIDs:
            agencies = " ".join(self._agencyIDs)
            seiscomp.logging.info(
                f" + agencyIDs filter for events and picks: {agencies}"
            )
        else:
            seiscomp.logging.info(" + agencyIDs: no filter is applied")

        if " ".join(self._authors):
            authors = " ".join(self._authors)
            seiscomp.logging.info(f" + Authors filter for events and picks: {authors}")
        else:
            seiscomp.logging.info(" + authors: no filter is applied")

        if " ".join(self._phaseHints):
            phaseHints = " ".join(self._phaseHints)
            seiscomp.logging.info(f" + phase hint filter for picks: '{phaseHints}'")
        else:
            seiscomp.logging.info(" + phase hints: no filter is applied")

        if " ".join(self._phaseStreams):
            streams = " ".join(self._phaseStreams)
            seiscomp.logging.info(f" + phase stream ID filter for picks: '{streams}'")
        else:
            seiscomp.logging.info(" + phase stream ID: no filter is applied")

        return True

    def run(self):
        try:
            try:
                eventID = self.commandline().optionString("eventid")
                event = self._cache.get(seiscomp.datamodel.Event, eventID)
                if event:
                    self.notifyEvent(event)
            except RuntimeError:
                pass

            return seiscomp.client.Application.run(self)
        except Exception:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            return False

    def done(self):
        self._cache = None
        seiscomp.client.Application.done(self)

    def runPickScript(self, pickObjectList):
        if not self._pickScript:
            return

        for pickObject in pickObjectList:
            # parse values
            try:
                net = pickObject.waveformID().networkCode()
            except Exception:
                net = "unknown"
            try:
                sta = pickObject.waveformID().stationCode()
            except Exception:
                sta = "unknown"
            pickID = pickObject.publicID()
            try:
                phaseHint = pickObject.phaseHint().code()
            except Exception:
                phaseHint = "unknown"

            print(net, sta, pickID, phaseHint, file=sys.stderr)

            if self._pickProc is not None:
                if self._pickProc.poll() is None:
                    seiscomp.logging.info(
                        "Pick script still in progress -> wait one second"
                    )
                    self._pickProc.wait(1)
                if self._pickProc.poll() is None:
                    seiscomp.logging.warning(
                        "Pick script still in progress -> skipping message"
                    )
                    return
            try:
                self._pickProc = subprocess.Popen(
                    [self._pickScript, net, sta, pickID, phaseHint]
                )
                seiscomp.logging.info(
                    f"Started pick script with pid {self._pickProc.pid}"
                )
            except Exception:
                seiscomp.logging.error(
                    f"Failed to start pick script '{self._pickScript}'"
                )

    def runAmpScript(self, ampObject):
        if not self._ampScript:
            return

        # parse values
        net = ampObject.waveformID().networkCode()
        sta = ampObject.waveformID().stationCode()
        amp = ampObject.amplitude().value()
        ampID = ampObject.publicID()

        if self._ampProc is not None:
            if self._ampProc.poll() is None:
                seiscomp.logging.warning(
                    "Amplitude script still in progress -> skipping message"
                )
                return
        try:
            self._ampProc = subprocess.Popen(
                [self._ampScript, net, sta, f"{amp:.2f}", ampID]
            )
            seiscomp.logging.info(
                f"Started amplitude script with pid {self._ampProc.pid}"
            )
        except Exception:
            seiscomp.logging.error(
                f"Failed to start amplitude script '{self._ampScript}'"
            )

    def runAlert(self, lat, lon):
        if not self._alertScript:
            return

        if self._alertProc is not None:
            if self._alertProc.poll() is None:
                seiscomp.logging.warning(
                    "AlertScript still in progress -> skipping message"
                )
                return
        try:
            self._alertProc = subprocess.Popen(
                [self._alertScript, f"{lat:.1f}", f"{lon:.1f}"]
            )
            seiscomp.logging.info(
                f"Started alert script with pid {self._alertProc.pid}"
            )
        except Exception:
            seiscomp.logging.error(
                f"Failed to start alert script '{self._alertScript}'"
            )

    def handleMessage(self, msg):
        try:
            dm = seiscomp.core.DataMessage.Cast(msg)
            if dm:
                for att in dm:
                    org = seiscomp.datamodel.Origin.Cast(att)
                    if org:
                        try:
                            if org.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                                self.runAlert(
                                    org.latitude().value(), org.longitude().value()
                                )
                        except Exception:
                            pass

            # ao = seiscomp.datamodel.ArtificialOriginMessage.Cast(msg)
            # if ao:
            #  org = ao.origin()
            #  if org:
            #    self.runAlert(org.latitude().value(), org.longitude().value())
            #  return

            seiscomp.client.Application.handleMessage(self, msg)
        except Exception:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def addObject(self, parentID, scObject):
        try:
            # pick
            obj = seiscomp.datamodel.Pick.Cast(scObject)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug(f"got new pick '{obj.publicID()}'")
                agencyID = obj.creationInfo().agencyID()
                author = obj.creationInfo().author()
                phaseHint = obj.phaseHint().code()
                if self._phaseStreams:
                    waveformID = "%s.%s.%s.%s" % (
                        obj.waveformID().networkCode(),
                        obj.waveformID().stationCode(),
                        obj.waveformID().locationCode(),
                        obj.waveformID().channelCode(),
                    )
                    matched = False
                    for rule in self._phaseStreams:
                        if re.fullmatch(rule, waveformID):
                            matched = True
                            break
                    if not matched:
                        seiscomp.logging.debug(
                            f" + stream ID {waveformID} does not match constraints.phaseStreams rules"
                        )
                        return

                if not self._agencyIDs or agencyID in self._agencyIDs:
                    if not self._phaseHints or phaseHint in self._phaseHints:
                        self.notifyPick(obj)
                    else:
                        seiscomp.logging.debug(
                            f" + phase hint {phaseHint} does not match '{self._phaseHints}'"
                        )
                else:
                    seiscomp.logging.debug(
                        f" + agencyID {agencyID} does not match '{self._agencyIDs}'"
                    )
                return

            # amplitude
            obj = seiscomp.datamodel.Amplitude.Cast(scObject)
            if obj:
                if obj.type() == self._ampType:
                    seiscomp.logging.debug(
                        f"got new {self._ampType} amplitude '{obj.publicID()}'"
                    )
                    self.notifyAmplitude(obj)
                return

            # origin
            obj = seiscomp.datamodel.Origin.Cast(scObject)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug(f"got new origin '{obj.publicID()}'")

                try:
                    if obj.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                        self.runAlert(obj.latitude().value(), obj.longitude().value())
                except Exception:
                    pass

                return

            # magnitude
            obj = seiscomp.datamodel.Magnitude.Cast(scObject)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug(f"got new magnitude '{obj.publicID()}'")
                return

            # event
            obj = seiscomp.datamodel.Event.Cast(scObject)
            if obj:
                org = self._cache.get(
                    seiscomp.datamodel.Origin, obj.preferredOriginID()
                )
                agencyID = org.creationInfo().agencyID()
                author = org.creationInfo().author()
                seiscomp.logging.debug(f"got new event '{obj.publicID()}'")
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    if not self._authors or author in self._authors:
                        self.notifyEvent(obj, True)
                return
        except Exception:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def updateObject(self, parentID, scObject):
        try:
            obj = seiscomp.datamodel.Event.Cast(scObject)
            if obj:
                org = self._cache.get(
                    seiscomp.datamodel.Origin, obj.preferredOriginID()
                )
                agencyID = org.creationInfo().agencyID()
                author = org.creationInfo().author()
                seiscomp.logging.debug(f"update event '{obj.publicID()}'")
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    if not self._authors or author in self._authors:
                        self.notifyEvent(obj, False)
        except Exception:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def handleTimeout(self):
        self.checkEnoughPicks()

    def checkEnoughPicks(self):
        if self._pickCache.size() >= self._phaseNumber:
            # wait until self._phaseInterval has elapsed before calling the
            # script (more picks might come)
            timeWindowLength = (
                seiscomp.core.Time.GMT() - self._pickCache.oldest()
            ).length()
            if timeWindowLength >= self._phaseInterval:
                picks = [seiscomp.datamodel.Pick.Cast(o) for o in self._pickCache]
                self.runPickScript(picks)
                self._pickCache.clear()

    def notifyPick(self, pick):
        if self._phaseNumber <= 1:
            self.runPickScript([pick])
        else:
            self.checkEnoughPicks()
            self._pickCache.feed(pick)

    def notifyAmplitude(self, amp):
        self.runAmpScript(amp)

    def notifyEvent(self, evt, newEvent=True, dtmax=3600):
        try:
            org = self._cache.get(seiscomp.datamodel.Origin, evt.preferredOriginID())
            if not org:
                seiscomp.logging.warning(
                    f"unable to get origin {evt.preferredOriginID()}, ignoring event message"
                )
                return

            preliminary = False
            try:
                if org.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                    preliminary = True
            except Exception:
                pass

            if preliminary is False:
                nmag = self._cache.get(
                    seiscomp.datamodel.Magnitude, evt.preferredMagnitudeID()
                )
                if nmag:
                    mag = nmag.magnitude().value()
                    mag = f"magnitude {mag:.1f}"
                else:
                    if len(evt.preferredMagnitudeID()) > 0:
                        seiscomp.logging.warning(
                            f"unable to get magnitude {evt.preferredMagnitudeID()}, "
                            "ignoring event message"
                        )
                    else:
                        seiscomp.logging.warning(
                            "no preferred magnitude yet, ignoring event message"
                        )
                    return

            # keep track of old events
            if self._newWhenFirstSeen:
                if evt.publicID() in self._oldEvents:
                    newEvent = False
                else:
                    newEvent = True
                self._oldEvents.append(evt.publicID())

            dsc = seiscomp.seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value()
            )

            if self._eventDescriptionPattern:
                try:
                    city, dist, azi = self.nearestCity(
                        org.latitude().value(),
                        org.longitude().value(),
                        self._citiesMaxDist,
                        self._citiesMinPopulation,
                    )
                    if city:
                        dsc = self._eventDescriptionPattern
                        region = seiscomp.seismology.Regions.getRegionName(
                            org.latitude().value(), org.longitude().value()
                        )
                        distStr = str(int(seiscomp.math.deg2km(dist)))
                        dsc = (
                            dsc.replace("@region@", region)
                            .replace("@dist@", distStr)
                            .replace("@poi@", city.name())
                        )
                except Exception:
                    pass

            seiscomp.logging.debug(f"desc: {dsc}")

            dep = org.depth().value()
            now = seiscomp.core.Time.GMT()
            otm = org.time().value()

            dt = (now - otm).seconds()

            #   if dt > dtmax:
            #       return

            if dt > 3600:
                dt = f"{int(dt / 3600)} hours {int((dt % 3600) / 60)} minutes ago"
            elif dt > 120:
                dt = f"{int(dt / 60)} minutes ago"
            else:
                dt = f"{int(dt)} seconds ago"

            if preliminary:
                message = f"earthquake, XXL, preliminary, {dt}, {dsc}"
            else:
                message = "earthquake, %s, %s, %s, depth %d kilometers" % (
                    dt,
                    dsc,
                    mag,
                    int(dep + 0.5),
                )
            seiscomp.logging.info(message)

            if not self._eventScript:
                return

            if self._eventProc is not None:
                if self._eventProc.poll() is None:
                    seiscomp.logging.warning(
                        "EventScript still in progress -> skipping message"
                    )
                    return

            try:
                param2 = 0
                param3 = 0
                param4 = ""
                if newEvent:
                    param2 = 1

                org = self._cache.get(
                    seiscomp.datamodel.Origin, evt.preferredOriginID()
                )
                if org:
                    try:
                        param3 = org.quality().associatedPhaseCount()
                    except Exception:
                        pass

                nmag = self._cache.get(
                    seiscomp.datamodel.Magnitude, evt.preferredMagnitudeID()
                )
                if nmag:
                    param4 = f"{nmag.magnitude().value():.1f}"

                self._eventProc = subprocess.Popen(
                    [
                        self._eventScript,
                        message,
                        "%d" % param2,
                        evt.publicID(),
                        "%d" % param3,
                        param4,
                    ]
                )
                seiscomp.logging.info(
                    f"Started event script with pid {self._eventProc.pid}"
                )
            except Exception:
                seiscomp.logging.error(
                    f"Failed to start event script '{self._eventScript} {message} "
                    f"{param2} {param3} {param4}'"
                )
        except Exception:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def printUsage(self):
        print(
            """Usage:
  scalert [options]

Execute custom scripts upon arrival of objects or updates"""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            """Examples:
Execute scalert on command line with debug output
  scalert --debug
"""
        )


app = ObjectAlert(len(sys.argv), sys.argv)
sys.exit(app())
