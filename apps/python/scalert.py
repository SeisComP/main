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
import seiscomp.core, seiscomp.client, seiscomp.datamodel, seiscomp.math
import seiscomp.logging, seiscomp.seismology, seiscomp.system


class ObjectAlert(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setLoadRegionsEnabled(True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(
            seiscomp.client.Protocol.LISTENER_GROUP)
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
        self._phaseHints = []
        self._phaseStreams = []
        self._phaseNumber = 1
        self._phaseInterval = 1

    def createCommandLineDescription(self):
        self.commandline().addOption("Generic", "first-new",
                                     "calls an event a new event when it is seen the first time")
        self.commandline().addGroup("Alert")
        self.commandline().addStringOption("Alert", "amp-type",
                                           "amplitude type to listen to", self._ampType)
        self.commandline().addStringOption("Alert", "pick-script",
                                           "script to be called when a pick arrived, network-, station code pick publicID are passed as parameters $1, $2, $3 and $4")
        self.commandline().addStringOption("Alert", "amp-script",
                                           "script to be called when a station amplitude arrived, network-, station code, amplitude and amplitude publicID are passed as parameters $1, $2, $3 and $4")
        self.commandline().addStringOption("Alert", "alert-script",
                                           "script to be called when a preliminary origin arrived, latitude and longitude are passed as parameters $1 and $2")
        self.commandline().addStringOption("Alert", "event-script",
                                           "script to be called when an event has been declared; the message string, a flag (1=new event, 0=update event), the EventID, the arrival count and the magnitude (optional when set) are passed as parameter $1, $2, $3, $4 and $5")
        self.commandline().addGroup("Cities")
        self.commandline().addStringOption("Cities", "max-dist",
                                           "maximum distance for using the distance from a city to the earthquake")
        self.commandline().addStringOption("Cities", "min-population",
                                           "minimum population for a city to become a point of interest")
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
        except:
            pass

        try:
            self._agencyIDs = [self.configGetString("agencyID")]
        except:
            pass

        try:
            agencyIDs = self.configGetStrings("agencyIDs")
            self._agencyIDs = []
            for item in agencyIDs:
                item = item.strip()
                if item not in self._agencyIDs:
                    self._agencyIDs.append(item)
        except:
            pass


        self._phaseHints = ['P','S']
        try:
            phaseHints = self.configGetStrings("constraints.phaseHints")
            self._phaseHints = []
            for item in phaseHints:
                item = item.strip()
                if item not in self._phaseHints:
                    self._phaseHints.append(item)
        except:
            pass

        self._phaseStreams = []
        try:
            phaseStreams = self.configGetStrings("constraints.phaseStreams")
            for item in phaseStreams:
                rule = item.strip()
                # rule is NET.STA.LOC.CHA and the special charactes ? * | ( ) are allowed
                if not re.fullmatch(r'[A-Z|a-z|0-9|\?|\*|\||\(|\)|\.]+', rule):
                    seiscomp.logging.error("Wrong stream ID format in `constraints.phaseStreams`: %s" % item)
                    return False
                # convert rule to a valid regular expression
                rule = re.sub(r'\.',  r'\.', rule)
                rule = re.sub(r'\?',  '.'  , rule)
                rule = re.sub(r'\*' , '.*' , rule)
                if rule not in self._phaseStreams:
                    self._phaseStreams.append(rule)
        except:
            pass

        try:
            self._phaseNumber = self.configGetInt("constraints.phaseNumber")
        except:
            pass

        try:
            self._phaseInterval = self.configGetInt("constraints.phaseInterval")
        except:
            pass

        if self._phaseNumber > 1:
            self._pickCache = seiscomp.datamodel.PublicObjectTimeSpanBuffer()
            self._pickCache.setTimeSpan(seiscomp.core.TimeSpan(self._phaseInterval))
            self.enableTimer(1)

        try:
            self._eventDescriptionPattern = self.configGetString("poi.message")
        except:
            pass

        try:
            self._citiesMaxDist = self.configGetDouble("poi.maxDist")
        except:
            pass

        try:
            self._citiesMinPopulation = self.configGetInt("poi.minPopulation")
        except:
            pass

        # mostly command-line options
        try:
            self._citiesMaxDist = self.commandline().optionDouble("max-dist")
        except:
            pass

        try:
            if self.commandline().hasOption("first-new"):
                self._newWhenFirstSeen = True
        except:
            pass

        try:
            self._citiesMinPopulation = self.commandline().optionInt("min-population")
        except:
            pass

        try:
            self._ampType = self.commandline().optionString("amp-type")
        except:
            pass

        try:
            self._pickScript = self.commandline().optionString("pick-script")
        except:
            try:
                self._pickScript = self.configGetString("scripts.pick")
            except:
                seiscomp.logging.warning("No pick script defined")

        if self._pickScript:
            self._pickScript = seiscomp.system.Environment.Instance().absolutePath(self._pickScript)
            seiscomp.logging.info("Using pick script %s" % self._pickScript)

            if not os.path.isfile(self._pickScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._pickScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._ampScript = self.commandline().optionString("amp-script")
        except:
            try:
                self._ampScript = self.configGetString("scripts.amplitude")
            except:
                seiscomp.logging.warning("No amplitude script defined")

        if self._ampScript:
            self._ampScript = seiscomp.system.Environment.Instance().absolutePath(self._ampScript)
            seiscomp.logging.info("Using amplitude script %s" % self._ampScript)

            if not os.path.isfile(self._ampScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._ampScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._alertScript = self.commandline().optionString("alert-script")
        except:
            try:
                self._alertScript = self.configGetString("scripts.alert")
            except:
                seiscomp.logging.warning("No alert script defined")

        if self._alertScript:
            self._alertScript = seiscomp.system.Environment.Instance(
            ).absolutePath(self._alertScript)
            seiscomp.logging.info("Using alert script %s" % self._alertScript)

            if not os.path.isfile(self._alertScript):
                seiscomp.logging.error(" + not exising")
                return False

            if not os.access(self._alertScript, os.X_OK):
                seiscomp.logging.error(" + not executable")
                return False

            foundScript = True

        try:
            self._eventScript = self.commandline().optionString("event-script")
        except:
            try:
                self._eventScript = self.configGetString("scripts.event")
            except:
                seiscomp.logging.warning("No event script defined")

        if self._eventScript:
            self._eventScript = seiscomp.system.Environment.Instance(
            ).absolutePath(self._eventScript)
            seiscomp.logging.info("Using event script %s" % self._eventScript)

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
            seiscomp.logging.warning(
                "No valid database interface to read from")
        self._cache = seiscomp.datamodel.PublicObjectRingBuffer(
            self.query(), 100)

        if self._ampScript and self.connection():
            seiscomp.logging.info(
                "Amplitude script defined: subscribing to AMPLITUDE message group")
            self.connection().subscribe("AMPLITUDE")

        if self._pickScript and self.connection():
            seiscomp.logging.info(
                "Pick script defined: subscribing to PICK message group")
            self.connection().subscribe("PICK")

        if self._newWhenFirstSeen:
            seiscomp.logging.info(
                "A new event is declared when I see it the first time")

        seiscomp.logging.info("Filtering:")
        if " ".join(self._agencyIDs):
            seiscomp.logging.info(" + agencyIDs filter for events and picks: %s" % (" ".join(self._agencyIDs)))
        else:
            seiscomp.logging.info(" + agencyIDs: no filter is applied")

        if " ".join(self._phaseHints):
            seiscomp.logging.info(" + phase hint filter for picks: '%s'" % (" ".join(self._phaseHints)))
        else:
            seiscomp.logging.info(" + phase hints: no filter is applied")

        if " ".join(self._phaseStreams):
            seiscomp.logging.info(" + phase stream ID filter for picks: '%s'" % (" ".join(self._phaseStreams)))
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
            except:
                pass

            return seiscomp.client.Application.run(self)
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            return False


    def runPickScript(self, pickObjectList):
        if not self._pickScript:
            return

        for pickObject in pickObjectList:
            # parse values
            try:
                net = pickObject.waveformID().networkCode()
            except:
                net = "unknown"
            try:
                sta = pickObject.waveformID().stationCode()
            except:
                sta = "unknown"
            pickID = pickObject.publicID()
            try:
                phaseHint =  pickObject.phaseHint().code()
            except:
                phaseHint = "unknown"

            print(net, sta, pickID, phaseHint)

            if self._pickProc is not None:
                if self._pickProc.poll() is None:
                    seiscomp.logging.info(
                            "Pick script still in progress -> wait one second")
                    self._pickProc.wait(1)
                if self._pickProc.poll() is None:
                    seiscomp.logging.warning(
                        "Pick script still in progress -> skipping message")
                    return
            try:
                self._pickProc = subprocess.Popen(
                    [self._pickScript, net, sta, pickID, phaseHint])
                seiscomp.logging.info(
                    "Started pick script with pid %d" % self._pickProc.pid)
            except:
                seiscomp.logging.error(
                    "Failed to start pick script '%s'" % self._pickScript)

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
                    "Amplitude script still in progress -> skipping message")
                return
        try:
            self._ampProc = subprocess.Popen(
                [self._ampScript, net, sta, "%.2f" % amp, ampID])
            seiscomp.logging.info(
                "Started amplitude script with pid %d" % self._ampProc.pid)
        except:
            seiscomp.logging.error(
                "Failed to start amplitude script '%s'" % self._ampScript)

    def runAlert(self, lat, lon):
        if not self._alertScript:
            return

        if self._alertProc is not None:
            if self._alertProc.poll() is None:
                seiscomp.logging.warning(
                    "AlertScript still in progress -> skipping message")
                return
        try:
            self._alertProc = subprocess.Popen(
                [self._alertScript, "%.1f" % lat, "%.1f" % lon])
            seiscomp.logging.info(
                "Started alert script with pid %d" % self._alertProc.pid)
        except:
            seiscomp.logging.error(
                "Failed to start alert script '%s'" % self._alertScript)

    def handleMessage(self, msg):
        try:
            dm = seiscomp.core.DataMessage.Cast(msg)
            if dm:
                for att in dm:
                    org = seiscomp.datamodel.Origin.Cast(att)
                    if org:
                        try:
                            if org.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                                self.runAlert(org.latitude().value(),
                                              org.longitude().value())
                        except:
                            pass

            #ao = seiscomp.datamodel.ArtificialOriginMessage.Cast(msg)
            # if ao:
            #  org = ao.origin()
            #  if org:
            #    self.runAlert(org.latitude().value(), org.longitude().value())
            #  return

            seiscomp.client.Application.handleMessage(self, msg)
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def addObject(self, parentID, object):
        try:
            # pick
            obj = seiscomp.datamodel.Pick.Cast(object)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug("got new pick '%s'" % obj.publicID())
                agencyID = obj.creationInfo().agencyID()
                phaseHint = obj.phaseHint().code()
                if  self._phaseStreams:
                    waveformID = "%s.%s.%s.%s" % (
                            obj.waveformID().networkCode(), obj.waveformID().stationCode(), 
                            obj.waveformID().locationCode(), obj.waveformID().channelCode())
                    matched = False
                    for rule in self._phaseStreams:
                        if re.fullmatch(rule, waveformID):
                            matched = True
                            break
                    if not matched:
                        seiscomp.logging.debug(
                                " + stream ID %s does not match constraints.phaseStreams rules"
                                 % (waveformID))
                        return
 
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    if not self._phaseHints or phaseHint in self._phaseHints:
                        self.notifyPick(obj)
                    else:
                        seiscomp.logging.debug(" + phase hint %s does not match '%s'"
                                               % (phaseHint, self._phaseHints))
                else:
                    seiscomp.logging.debug(" + agencyID %s does not match '%s'"
                                           % (agencyID, self._agencyIDs))
                return

            # amplitude
            obj = seiscomp.datamodel.Amplitude.Cast(object)
            if obj:
                if obj.type() == self._ampType:
                    seiscomp.logging.debug("got new %s amplitude '%s'" % (
                        self._ampType, obj.publicID()))
                    self.notifyAmplitude(obj)
                return

            # origin
            obj = seiscomp.datamodel.Origin.Cast(object)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug("got new origin '%s'" % obj.publicID())

                try:
                    if obj.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                        self.runAlert(obj.latitude().value(),
                                      obj.longitude().value())
                except:
                    pass

                return

            # magnitude
            obj = seiscomp.datamodel.Magnitude.Cast(object)
            if obj:
                self._cache.feed(obj)
                seiscomp.logging.debug(
                    "got new magnitude '%s'" % obj.publicID())
                return

            # event
            obj = seiscomp.datamodel.Event.Cast(object)
            if obj:
                org = self._cache.get(
                    seiscomp.datamodel.Origin, obj.preferredOriginID())
                agencyID = org.creationInfo().agencyID()
                seiscomp.logging.debug("got new event '%s'" % obj.publicID())
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    self.notifyEvent(obj, True)
                return
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def updateObject(self, parentID, object):
        try:
            obj = seiscomp.datamodel.Event.Cast(object)
            if obj:
                org = self._cache.get(
                    seiscomp.datamodel.Origin, obj.preferredOriginID())
                agencyID = org.creationInfo().agencyID()
                seiscomp.logging.debug("update event '%s'" % obj.publicID())
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    self.notifyEvent(obj, False)
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def handleTimeout(self):
        self.checkEnoughPicks()

    def checkEnoughPicks(self):
        if self._pickCache.size() >= self._phaseNumber:
            # wait until self._phaseInterval has elapsed before calling the
            # script (more picks might come)
            timeWindowLength = (seiscomp.core.Time.GMT() - self._pickCache.oldest()).length()
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
            org = self._cache.get(
                seiscomp.datamodel.Origin, evt.preferredOriginID())
            if not org:
                seiscomp.logging.warning(
                    "unable to get origin %s, ignoring event message" % evt.preferredOriginID())
                return

            preliminary = False
            try:
                if org.evaluationStatus() == seiscomp.datamodel.PRELIMINARY:
                    preliminary = True
            except:
                pass

            if preliminary == False:
                nmag = self._cache.get(
                    seiscomp.datamodel.Magnitude, evt.preferredMagnitudeID())
                if nmag:
                    mag = nmag.magnitude().value()
                    mag = "magnitude %.1f" % mag
                else:
                    if len(evt.preferredMagnitudeID()) > 0:
                        seiscomp.logging.warning(
                            "unable to get magnitude %s, ignoring event message" % evt.preferredMagnitudeID())
                    else:
                        seiscomp.logging.warning(
                            "no preferred magnitude yet, ignoring event message")
                    return

            # keep track of old events
            if self._newWhenFirstSeen:
                if evt.publicID() in self._oldEvents:
                    newEvent = False
                else:
                    newEvent = True
                self._oldEvents.append(evt.publicID())

            dsc = seiscomp.seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value())

            if self._eventDescriptionPattern:
                try:
                    city, dist, azi = self.nearestCity(org.latitude().value(), org.longitude(
                    ).value(), self._citiesMaxDist, self._citiesMinPopulation)
                    if city:
                        dsc = self._eventDescriptionPattern
                        region = seiscomp.seismology.Regions.getRegionName(
                            org.latitude().value(), org.longitude().value())
                        distStr = str(int(seiscomp.math.deg2km(dist)))
                        dsc = dsc.replace("@region@", region).replace(
                            "@dist@", distStr).replace("@poi@", city.name())
                except:
                    pass

            seiscomp.logging.debug("desc: %s" % dsc)

            dep = org.depth().value()
            now = seiscomp.core.Time.GMT()
            otm = org.time().value()

            dt = (now - otm).seconds()

    #   if dt > dtmax:
    #       return

            if dt > 3600:
                dt = "%d hours %d minutes ago" % (dt/3600, (dt % 3600)/60)
            elif dt > 120:
                dt = "%d minutes ago" % (dt/60)
            else:
                dt = "%d seconds ago" % dt

            if preliminary:
                message = "earthquake, XXL, preliminary, %s, %s" % (dt, dsc)
            else:
                message = "earthquake, %s, %s, %s, depth %d kilometers" % (
                    dt, dsc, mag, int(dep+0.5))
            seiscomp.logging.info(message)

            if not self._eventScript:
                return

            if self._eventProc is not None:
                if self._eventProc.poll() is None:
                    seiscomp.logging.warning(
                        "EventScript still in progress -> skipping message")
                    return

            try:
                param2 = 0
                param3 = 0
                param4 = ""
                if newEvent:
                    param2 = 1

                org = self._cache.get(
                    seiscomp.datamodel.Origin, evt.preferredOriginID())
                if org:
                    try:
                        param3 = org.quality().associatedPhaseCount()
                    except:
                        pass

                nmag = self._cache.get(
                    seiscomp.datamodel.Magnitude, evt.preferredMagnitudeID())
                if nmag:
                    param4 = "%.1f" % nmag.magnitude().value()

                self._eventProc = subprocess.Popen(
                    [self._eventScript, message, "%d" % param2, evt.publicID(), "%d" % param3, param4])
                seiscomp.logging.info(
                    "Started event script with pid %d" % self._eventProc.pid)
            except:
                seiscomp.logging.error("Failed to start event script '%s %s %d %d %s'" % (
                    self._eventScript, message, param2, param3, param4))
        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def printUsage(self):

        print('''Usage:
  scalert [options]

Execute custom scripts upon arrival of objects or updates''')

        seiscomp.client.Application.printUsage(self)

        print('''Examples:
Execute scalert on command line with debug output
  scalert --debug
''')

app = ObjectAlert(len(sys.argv), sys.argv)
sys.exit(app())
