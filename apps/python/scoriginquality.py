#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) Geoscience Australia                                        #
# All rights reserved.                                                      #
#                                                                           #
# GNU Affero General Public License Usage                                   #
# This file may be used under the terms of the GNU Affero                   #
# Public License version 3.0 as published by the Free Software Foundation   #
# and appearing in the file LICENSE included in the packaging of this       #
# file. Please review the following information to ensure the GNU Affero    #
# Public License version 3.0 requirements will be met:                      #
# https://www.gnu.org/licenses/agpl-3.0.html.                               #
############################################################################
#
# scoriginquality - Automatic A/B/C/D origin quality grader
#
# Two modes:
#
#   Live mode (default):
#     Subscribes to LOCATION messages, grades each incoming origin and
#     writes the grade back as a Comment via messaging.
#
#   Batch mode (--event / --ep):
#     --event <eventID>   Grade the preferred origin of one event from DB.
#     --ep <file>         Grade all origins in a SeisComP XML file.
#
# Grade thresholds (all configurable, defaults follow ISC/ANSS conventions):
#
#   Grade | maxGap | maxSecGap | maxRMS | minStations | maxMinDist
#   ------+--------+-----------+--------+-------------+-----------
#     A   |   90   |    135    |  0.15  |     10      |    30
#     B   |  135   |    180    |  0.30  |      6      |    60
#     C   |  180   |    210    |  0.50  |      4      |    90
#     D   |  anything worse than C                              |
############################################################################

import sys
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.io
import seiscomp.logging


GRADES = ("A", "B", "C", "D")


def _grade_index(value, thresholds):
    for i, threshold in enumerate(thresholds):
        if value <= threshold:
            return i
    return 3


def _grade_index_stations(nsta, min_thresholds):
    for i, threshold in enumerate(min_thresholds):
        if nsta >= threshold:
            return i
    return 3


class OriginQualityApp(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)
        self.addMessagingSubscription("LOCATION")

        self.setAutoApplyNotifierEnabled(True)
        self.setInterpretNotifierEnabled(True)

        self._commentID   = "quality"
        self._eventID     = None
        self._epFile      = None

        self._maxGap      = [90.0,  135.0, 180.0, 360.0]
        self._maxSecGap   = [135.0, 180.0, 210.0, 360.0]
        self._maxRMS      = [0.15,  0.30,  0.50,  1e9]
        self._minStations = [10,    6,     4,     1]
        self._maxMinDist  = [30.0,  60.0,  90.0,  180.0]

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def createCommandLineDescription(self):
        self.commandline().addGroup("Batch")
        self.commandline().addStringOption(
            "Batch", "event,E",
            "Grade the preferred origin of this event ID from the database and exit."
        )
        self.commandline().addStringOption(
            "Batch", "ep",
            "Grade all origins in a SeisComP XML file and print results to stdout."
        )
        self.commandline().addGroup("Quality")
        self.commandline().addStringOption(
            "Quality", "comment-id",
            "Comment ID written to the origin (default: quality)."
        )

    def initConfiguration(self):
        if not seiscomp.client.Application.initConfiguration(self):
            return False

        try:
            self._commentID = self.configGetString("quality.commentID")
        except Exception:
            pass

        for grade_idx, grade in enumerate(("A", "B", "C")):
            for attr, lst in (("maxGap",        self._maxGap),
                              ("maxSecondaryGap", self._maxSecGap),
                              ("maxRMS",         self._maxRMS),
                              ("maxMinDist",     self._maxMinDist)):
                try:
                    lst[grade_idx] = self.configGetDouble(
                        "quality.%s.%s" % (grade, attr))
                except Exception:
                    pass
            try:
                self._minStations[grade_idx] = self.configGetInt(
                    "quality.%s.minStations" % grade)
            except Exception:
                pass

        return True

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        try:
            self._commentID = self.commandline().optionString("comment-id")
        except Exception:
            pass

        try:
            self._eventID = self.commandline().optionString("event")
        except Exception:
            pass

        try:
            self._epFile = self.commandline().optionString("ep")
        except Exception:
            pass

        seiscomp.logging.info(
            "scoriginquality: writing grade to comment '%s'" % self._commentID
        )
        return True

    # ------------------------------------------------------------------
    # Entry point — batch modes short-circuit the messaging loop
    # ------------------------------------------------------------------

    def run(self):
        if self._eventID:
            return self._runEventMode(self._eventID)
        if self._epFile:
            return self._runEpMode(self._epFile)
        return seiscomp.client.Application.run(self)

    # ------------------------------------------------------------------
    # Batch: single event from database
    # ------------------------------------------------------------------

    def _runEventMode(self, eventID):
        if not self.query():
            seiscomp.logging.error("No database connection")
            return False

        obj = self.query().loadObject(seiscomp.datamodel.Event.TypeInfo(), eventID)
        event = seiscomp.datamodel.Event.Cast(obj)
        if not event:
            seiscomp.logging.error("Event '%s' not found in database" % eventID)
            return False

        originID = event.preferredOriginID()
        if not originID:
            seiscomp.logging.error("Event '%s' has no preferred origin" % eventID)
            return False

        obj = self.query().loadObject(seiscomp.datamodel.Origin.TypeInfo(), originID)
        origin = seiscomp.datamodel.Origin.Cast(obj)
        if not origin:
            seiscomp.logging.error("Origin '%s' not found in database" % originID)
            return False

        grade, details = self._computeGrade(origin)
        if grade is None:
            seiscomp.logging.error(
                "Origin '%s' has no usable OriginQuality — cannot grade" % originID
            )
            return False

        self._logGrade(originID, grade, details)
        self._sendGrade(origin, grade, details)
        self.connection().syncOutbox()
        return True

    # ------------------------------------------------------------------
    # Batch: XML event parameters file
    # ------------------------------------------------------------------

    def _runEpMode(self, filename):
        ar = seiscomp.io.XMLArchive()
        if not ar.open(filename):
            seiscomp.logging.error("Cannot open file '%s'" % filename)
            return False

        ep = seiscomp.datamodel.EventParameters.Cast(ar.readObject())
        ar.close()
        if not ep:
            seiscomp.logging.error("No EventParameters found in '%s'" % filename)
            return False

        for i in range(ep.originCount()):
            origin = ep.origin(i)
            grade, details = self._computeGrade(origin)
            if grade is None:
                seiscomp.logging.warning(
                    "Origin '%s' has no usable OriginQuality — skipping"
                    % origin.publicID()
                )
                continue
            self._logGrade(origin.publicID(), grade, details)
            # In --ep mode we have no DB to check for existing comments,
            # so always OP_ADD
            self._sendGrade(origin, grade, details, force_add=True)

        self.connection().syncOutbox()
        return True

    # ------------------------------------------------------------------
    # Live messaging handlers
    # ------------------------------------------------------------------

    def addObject(self, parentID, obj):
        origin = seiscomp.datamodel.Origin.Cast(obj)
        if origin:
            self._processOrigin(origin)

    def updateObject(self, parentID, obj):
        origin = seiscomp.datamodel.Origin.Cast(obj)
        if origin:
            self._processOrigin(origin)

    def _processOrigin(self, origin):
        grade, details = self._computeGrade(origin)
        if grade is None:
            seiscomp.logging.warning(
                "scoriginquality: %s has no usable OriginQuality — skipping"
                % origin.publicID()
            )
            return
        self._logGrade(origin.publicID(), grade, details)
        self._sendGrade(origin, grade, details)

    # ------------------------------------------------------------------
    # Core grading logic
    # ------------------------------------------------------------------

    def _computeGrade(self, origin):
        try:
            oq = origin.quality()
        except Exception:
            return None, {}

        indices = []
        details = {}

        try:
            gap = oq.azimuthalGap()
            idx = _grade_index(gap, self._maxGap)
            indices.append(idx)
            details["gap"] = (gap, GRADES[idx])
        except Exception:
            pass

        try:
            sgap = oq.secondaryAzimuthalGap()
            idx = _grade_index(sgap, self._maxSecGap)
            indices.append(idx)
            details["secGap"] = (sgap, GRADES[idx])
        except Exception:
            pass

        try:
            rms = oq.standardError()
            idx = _grade_index(rms, self._maxRMS)
            indices.append(idx)
            details["rms"] = (rms, GRADES[idx])
        except Exception:
            pass

        try:
            nsta = oq.usedStationCount()
            idx = _grade_index_stations(nsta, self._minStations)
            indices.append(idx)
            details["stations"] = (nsta, GRADES[idx])
        except Exception:
            pass

        try:
            minDist = oq.minimumDistance()
            idx = _grade_index(minDist, self._maxMinDist)
            indices.append(idx)
            details["minDist"] = (minDist, GRADES[idx])
        except Exception:
            pass

        if not indices:
            return None, {}

        return GRADES[max(indices)], details

    def _logGrade(self, originID, grade, details):
        detail_str = ", ".join(
            "%s=%.4g(%s)" % (k, v[0], v[1]) for k, v in details.items()
        )
        seiscomp.logging.info(
            "scoriginquality: %s -> %s [%s]" % (originID, grade, detail_str)
        )

    def _buildCommentText(self, grade, details):
        units  = {"gap": "°", "secGap": "°", "rms": " s", "stations": "", "minDist": "°"}
        labels = {"gap": "Az. Gap", "secGap": "Sec. Gap", "rms": "RMS",
                  "stations": "Stations", "minDist": "Min. Dist"}
        fmts   = {"gap": "%.1f", "secGap": "%.1f", "rms": "%.3f",
                  "stations": "%d", "minDist": "%.1f"}
        lines = [grade]
        for key in ("gap", "secGap", "rms", "stations", "minDist"):
            if key not in details:
                continue
            val, g = details[key]
            valstr = fmts[key] % val
            lines.append("%s: %s%s \u2192 %s" % (labels[key], valstr, units[key], g))
        return "\n".join(lines)

    def _sendGrade(self, origin, grade, details=None, force_add=False):
        comment = seiscomp.datamodel.Comment()
        comment.setId(self._commentID)
        comment.setText(self._buildCommentText(grade, details or {}))
        ci = seiscomp.datamodel.CreationInfo()
        ci.setAgencyID(self.agencyID())
        ci.setAuthor("scoriginquality")
        ci.setCreationTime(seiscomp.core.Time.GMT())
        comment.setCreationInfo(ci)

        op = seiscomp.datamodel.OP_ADD
        if not force_add:
            for i in range(origin.commentCount()):
                try:
                    if origin.comment(i).id() == self._commentID:
                        op = seiscomp.datamodel.OP_UPDATE
                        break
                except Exception:
                    pass

        msg = seiscomp.datamodel.NotifierMessage()
        msg.attach(seiscomp.datamodel.Notifier(origin.publicID(), op, comment))

        if not self.connection().send("LOCATION", msg):
            seiscomp.logging.error(
                "scoriginquality: failed to send grade for %s"
                % origin.publicID()
            )


def main():
    app = OriginQualityApp(len(sys.argv), sys.argv)
    return app()


if __name__ == "__main__":
    sys.exit(main())
