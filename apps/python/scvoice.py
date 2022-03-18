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
import subprocess
import traceback

from seiscomp import (client, core, datamodel, logging, seismology, system,
                      math)

class VoiceAlert(client.Application):

    def __init__(self, argc, argv):
        client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setLoadRegionsEnabled(True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(client.Protocol.LISTENER_GROUP)
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

        self._cache = None
        self._eventDescriptionPattern = None
        self._ampScript = None
        self._alertScript = None
        self._eventScript = None

        self._ampProc = None
        self._alertProc = None
        self._eventProc = None

        self._newWhenFirstSeen = False
        self._prevMessage = {}
        self._agencyIDs = []

    def createCommandLineDescription(self):
        self.commandline().addOption(
            "Generic", "first-new", "calls an event a new event when it is "
            "seen the first time")
        self.commandline().addGroup("Alert")
        self.commandline().addStringOption(
            "Alert", "amp-type", "specify the amplitude type to listen to",
            self._ampType)
        self.commandline().addStringOption(
            "Alert", "amp-script", "specify the script to be called when a "
            "stationamplitude arrived, network-, stationcode and amplitude are "
            "passed as parameters $1, $2 and $3")
        self.commandline().addStringOption(
            "Alert", "alert-script", "specify the script to be called when a "
            "preliminary origin arrived, latitude and longitude are passed as "
            "parameters $1 and $2")
        self.commandline().addStringOption(
            "Alert", "event-script", "specify the script to be called when an "
            "event has been declared; the message string, a flag (1=new event, "
            "0=update event), the EventID, the arrival count and the magnitude "
            "(optional when set) are passed as parameter $1, $2, $3, $4 and $5")
        self.commandline().addGroup("Cities")
        self.commandline().addStringOption(
            "Cities", "max-dist", "maximum distance for using the distance "
            "from a city to the earthquake")
        self.commandline().addStringOption(
            "Cities", "min-population", "minimum population for a city to "
            "become a point of interest")
        self.commandline().addGroup("Debug")
        self.commandline().addStringOption(
            "Debug", "eventid,E", "specify Event ID")
        return True

    def init(self):
        if not client.Application.init(self):
            return False

        try:
            self._newWhenFirstSeen = self.configGetBool("firstNew")
        except BaseException:
            pass

        try:
            agencyIDs = self.configGetStrings("agencyIDs")
            for item in agencyIDs:
                item = item.strip()
                if item not in self._agencyIDs:
                    self._agencyIDs.append(item)
        except BaseException:
            pass

        try:
            if self.commandline().hasOption("first-new"):
                self._newWhenFirstSeen = True
        except BaseException:
            pass

        try:
            self._eventDescriptionPattern = self.configGetString("poi.message")
        except BaseException:
            pass

        try:
            self._citiesMaxDist = self.configGetDouble("poi.maxDist")
        except BaseException:
            pass

        try:
            self._citiesMaxDist = self.commandline().optionDouble("max-dist")
        except BaseException:
            pass

        try:
            self._citiesMinPopulation = self.configGetInt("poi.minPopulation")
        except BaseException:
            pass

        try:
            self._citiesMinPopulation = self.commandline().optionInt("min-population")
        except BaseException:
            pass

        try:
            self._ampType = self.commandline().optionString("amp-type")
        except BaseException:
            pass

        try:
            self._ampScript = self.commandline().optionString("amp-script")
        except BaseException:
            try:
                self._ampScript = self.configGetString("scripts.amplitude")
            except BaseException:
                logging.warning("No amplitude script defined")

        if self._ampScript:
            self._ampScript = system.Environment.Instance().absolutePath(self._ampScript)

        try:
            self._alertScript = self.commandline().optionString("alert-script")
        except BaseException:
            try:
                self._alertScript = self.configGetString("scripts.alert")
            except BaseException:
                logging.warning("No alert script defined")

        if self._alertScript:
            self._alertScript = system.Environment.Instance(
            ).absolutePath(self._alertScript)

        try:
            self._eventScript = self.commandline().optionString("event-script")
        except BaseException:
            try:
                self._eventScript = self.configGetString("scripts.event")
                logging.info(
                    "Using event script: %s" % self._eventScript)
            except BaseException:
                logging.warning("No event script defined")

        if self._eventScript:
            self._eventScript = system.Environment.Instance() \
                                .absolutePath(self._eventScript)

        logging.info("Creating ringbuffer for 100 objects")
        if not self.query():
            logging.warning(
                "No valid database interface to read from")
        self._cache = datamodel.PublicObjectRingBuffer(
            self.query(), 100)

        if self._ampScript and self.connection():
            self.connection().subscribe("AMPLITUDE")

        if self._newWhenFirstSeen:
            logging.info(
                "A new event is declared when I see it the first time")

        if not self._agencyIDs:
            logging.info("agencyIDs: []")
        else:
            logging.info(
                "agencyIDs: %s" % (" ".join(self._agencyIDs)))

        return True
    
    def printUsage(self):

        print('''Usage:
  scvoice [options]

Alert the user acoustically in real time.
''')

        client.Application.printUsage(self)

        print('''Examples:
Execute scvoice on command line with debug output
  scvoice --debug
''')

    def run(self):
        try:
            try:
                eventID = self.commandline().optionString("eventid")
                event = self._cache.get(datamodel.Event, eventID)
                if event:
                    self.notifyEvent(event)
            except BaseException:
                pass

            return client.Application.run(self)
        except BaseException:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            return False

    def runAmpScript(self, net, sta, amp):
        if not self._ampScript:
            return

        if self._ampProc is not None:
            if self._ampProc.poll() is None:
                logging.warning(
                    "AmplitudeScript still in progress -> skipping message")
                return
        try:
            self._ampProc = subprocess.Popen(
                [self._ampScript, net, sta, "%.2f" % amp])
            logging.info(
                "Started amplitude script with pid %d" % self._ampProc.pid)
        except BaseException:
            logging.error(
                "Failed to start amplitude script '%s'" % self._ampScript)

    def runAlert(self, lat, lon):
        if not self._alertScript:
            return

        if self._alertProc is not None:
            if self._alertProc.poll() is None:
                logging.warning(
                    "AlertScript still in progress -> skipping message")
                return
        try:
            self._alertProc = subprocess.Popen(
                [self._alertScript, "%.1f" % lat, "%.1f" % lon])
            logging.info(
                "Started alert script with pid %d" % self._alertProc.pid)
        except BaseException:
            logging.error(
                "Failed to start alert script '%s'" % self._alertScript)

    def handleMessage(self, msg):
        try:
            dm = core.DataMessage.Cast(msg)
            if dm:
                for att in dm:
                    org = datamodel.Origin.Cast(att)
                    if not org:
                        continue

                    try:
                        if org.evaluationStatus() == datamodel.PRELIMINARY:
                            self.runAlert(org.latitude().value(),
                                          org.longitude().value())
                    except BaseException:
                        pass

            #ao = datamodel.ArtificialOriginMessage.Cast(msg)
            # if ao:
            #  org = ao.origin()
            #  if org:
            #    self.runAlert(org.latitude().value(), org.longitude().value())
            #  return

            client.Application.handleMessage(self, msg)
        except BaseException:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def addObject(self, parentID, arg0):
        #pylint: disable=W0622
        try:
            obj = datamodel.Amplitude.Cast(arg0)
            if obj:
                if obj.type() == self._ampType:
                    logging.debug("got new %s amplitude '%s'" % (
                        self._ampType, obj.publicID()))
                    self.notifyAmplitude(obj)

            obj = datamodel.Origin.Cast(arg0)
            if obj:
                self._cache.feed(obj)
                logging.debug("got new origin '%s'" % obj.publicID())

                try:
                    if obj.evaluationStatus() == datamodel.PRELIMINARY:
                        self.runAlert(obj.latitude().value(),
                                      obj.longitude().value())
                except BaseException:
                    pass

                return

            obj = datamodel.Magnitude.Cast(arg0)
            if obj:
                self._cache.feed(obj)
                logging.debug(
                    "got new magnitude '%s'" % obj.publicID())
                return

            obj = datamodel.Event.Cast(arg0)
            if obj:
                org = self._cache.get(
                    datamodel.Origin, obj.preferredOriginID())
                agencyID = org.creationInfo().agencyID()
                logging.debug("got new event '%s'" % obj.publicID())
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    self.notifyEvent(obj, True)
        except BaseException:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def updateObject(self, parentID, arg0):
        try:
            obj = datamodel.Event.Cast(arg0)
            if obj:
                org = self._cache.get(datamodel.Origin, obj.preferredOriginID())
                agencyID = org.creationInfo().agencyID()
                logging.debug("update event '%s'" % obj.publicID())
                if not self._agencyIDs or agencyID in self._agencyIDs:
                    self.notifyEvent(obj, False)
        except BaseException:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)

    def notifyAmplitude(self, amp):
        self.runAmpScript(amp.waveformID().networkCode(),
                          amp.waveformID().stationCode(),
                          amp.amplitude().value())

    def notifyEvent(self, evt, newEvent=True):
        try:
            org = self._cache.get(datamodel.Origin, evt.preferredOriginID())
            if not org:
                logging.warning("unable to get origin %s, ignoring event "
                                "message" % evt.preferredOriginID())
                return

            preliminary = False
            try:
                if org.evaluationStatus() == datamodel.PRELIMINARY:
                    preliminary = True
            except BaseException:
                pass

            if not preliminary:
                nmag = self._cache.get(
                    datamodel.Magnitude, evt.preferredMagnitudeID())
                if nmag:
                    mag = nmag.magnitude().value()
                    mag = "magnitude %.1f" % mag
                else:
                    if len(evt.preferredMagnitudeID()) > 0:
                        logging.warning(
                            "unable to get magnitude %s, ignoring event "
                            "message" % evt.preferredMagnitudeID())
                    else:
                        logging.warning(
                            "no preferred magnitude yet, ignoring event message")
                    return

            # keep track of old events
            if self._newWhenFirstSeen:
                if evt.publicID() in self._prevMessage:
                    newEvent = False
                else:
                    newEvent = True

            dsc = seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value())

            if self._eventDescriptionPattern:
                try:
                    city, dist, _ = self.nearestCity(
                        org.latitude().value(), org.longitude().value(),
                        self._citiesMaxDist, self._citiesMinPopulation)
                    if city:
                        dsc = self._eventDescriptionPattern
                        region = seismology.Regions.getRegionName(
                            org.latitude().value(), org.longitude().value())
                        distStr = str(int(math.deg2km(dist)))
                        dsc = dsc.replace("@region@", region).replace(
                            "@dist@", distStr).replace("@poi@", city.name())
                except BaseException:
                    pass

            logging.debug("desc: %s" % dsc)

            dep = org.depth().value()
            now = core.Time.GMT()
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
                message = "earthquake, preliminary, %%s, %s" % dsc
            else:
                message = "earthquake, %%s, %s, %s, depth %d kilometers" % (
                    dsc, mag, int(dep+0.5))
            # at this point the message lacks the "ago" part

            if evt.publicID() in self._prevMessage and \
               self._prevMessage[evt.publicID()] == message:
                logging.info("Suppressing repeated message '%s'" % message)
                return

            self._prevMessage[evt.publicID()] = message
            message = message % dt  # fill the "ago" part
            logging.info(message)

            if not self._eventScript:
                return

            if self._eventProc is not None:
                if self._eventProc.poll() is None:
                    logging.warning(
                        "EventScript still in progress -> skipping message")
                    return

            try:
                param2 = 0
                param3 = 0
                param4 = ""
                if newEvent:
                    param2 = 1

                org = self._cache.get(
                    datamodel.Origin, evt.preferredOriginID())
                if org:
                    try:
                        param3 = org.quality().associatedPhaseCount()
                    except BaseException:
                        pass

                nmag = self._cache.get(
                    datamodel.Magnitude, evt.preferredMagnitudeID())
                if nmag:
                    param4 = "%.1f" % nmag.magnitude().value()

                self._eventProc = subprocess.Popen(
                    [self._eventScript, message, "%d" % param2, evt.publicID(),
                     "%d" % param3, param4])
                logging.info(
                    "Started event script with pid %d" % self._eventProc.pid)
            except BaseException:
                logging.error(
                    "Failed to start event script '%s %s %d %d %s'" % (
                        self._eventScript, message, param2, param3, param4))
        except BaseException:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)


app = VoiceAlert(len(sys.argv), sys.argv)
sys.exit(app())
