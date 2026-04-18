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
# Grading uses a worst-of rule across two categories:
#
#   Network geometry (statistical proxies for constraint quality):
#     azimuthal gap, secondary azimuthal gap, station count, min distance
#
#   Locator output (direct measures of location quality):
#     RMS residual, horizontal uncertainty (semi-major axis), depth
#     uncertainty, ground truth level (iLoc GT0-GT25)
#
# All thresholds are configurable. Parameters not present in the data are
# silently skipped — the grade is based on whatever is available.
#
# Default thresholds:
#
#   Grade | Gap   | SecGap | RMS    | Sta | MinDist | HorizUnc | DepUnc | GT
#   ------+-------+--------+--------+-----+---------+----------+--------+------
#     A   |  90°  |  135°  | 0.15s  | ≥10 |   30°   |   5 km   |  5 km  | ≤GT2
#     B   | 135°  |  180°  | 0.30s  |  ≥6 |   60°   |  10 km   | 15 km  | ≤GT5
#     C   | 180°  |  210°  | 0.50s  |  ≥4 |   90°   |  20 km   | 30 km  | ≤GT10
#     D   | anything worse than C                                               |
############################################################################

import sys
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.io
import seiscomp.logging


GRADES = ("A", "B", "C", "D")

# iLoc ground truth levels in order from best to worst
_GT_ORDER = ["GT0", "GT1", "GT2", "GT5", "GT10", "GT25"]


def _grade_index(value, thresholds):
    """Return 0-3 (A-D) for a value where lower is worse (e.g. gap, RMS, uncertainty)."""
    for i, threshold in enumerate(thresholds):
        if value <= threshold:
            return i
    return 3


def _grade_index_stations(nsta, min_thresholds):
    """Return 0-3 (A-D) for station count where higher is better."""
    for i, threshold in enumerate(min_thresholds):
        if nsta >= threshold:
            return i
    return 3


def _grade_index_gt(gt_level):
    """Return 0-3 (A-D) for an iLoc ground truth level string."""
    if not gt_level:
        return None
    gt = gt_level.strip().upper()
    # GT0/GT1 → A, GT2 → B, GT5 → B, GT10 → C, GT25 → C, worse → D
    mapping = {"GT0": 0, "GT1": 0, "GT2": 1, "GT5": 1, "GT10": 2, "GT25": 2}
    return mapping.get(gt, 3)


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

        # --- Network geometry thresholds (statistical proxies) ---
        self._maxGap      = [90.0,  135.0, 180.0, 360.0]
        self._maxSecGap   = [135.0, 180.0, 210.0, 360.0]
        self._minStations = [10,    6,     4,     1]
        self._maxMinDist  = [30.0,  60.0,  90.0,  180.0]

        # --- Locator output thresholds (direct quality measures) ---
        self._maxRMS      = [0.15,  0.30,  0.50,  1e9]
        self._maxHorizUnc = [5.0,   10.0,  20.0,  1e9]   # km, semi-major axis
        self._maxDepUnc   = [5.0,   15.0,  30.0,  1e9]   # km

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

        scalar_params = [
            ("maxGap",            self._maxGap,      "double"),
            ("maxSecondaryGap",   self._maxSecGap,   "double"),
            ("maxRMS",            self._maxRMS,       "double"),
            ("maxMinDist",        self._maxMinDist,   "double"),
            ("maxHorizUncertainty", self._maxHorizUnc, "double"),
            ("maxDepthUncertainty", self._maxDepUnc,   "double"),
        ]

        for grade_idx, grade in enumerate(("A", "B", "C")):
            for param, lst, typ in scalar_params:
                try:
                    if typ == "double":
                        lst[grade_idx] = self.configGetDouble(
                            "quality.%s.%s" % (grade, param))
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
    # Entry point
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
                "Origin '%s' has no usable quality data — cannot grade" % originID
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
                    "Origin '%s' has no usable quality data — skipping"
                    % origin.publicID()
                )
                continue
            self._logGrade(origin.publicID(), grade, details)
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
                "scoriginquality: %s has no usable quality data — skipping"
                % origin.publicID()
            )
            return
        self._logGrade(origin.publicID(), grade, details)
        self._sendGrade(origin, grade, details)

    # ------------------------------------------------------------------
    # Core grading logic
    # ------------------------------------------------------------------

    def _computeGrade(self, origin):
        indices = []
        details = {}

        # --- OriginQuality fields ---
        try:
            oq = origin.quality()

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

            try:
                gt = oq.groundTruthLevel()
                idx = _grade_index_gt(gt)
                if idx is not None:
                    indices.append(idx)
                    details["GT"] = (gt, GRADES[idx])
            except Exception:
                pass

        except Exception:
            pass

        # --- OriginUncertainty fields (direct locator output) ---
        try:
            ou = origin.uncertainty()

            try:
                horiz = ou.maxHorizontalUncertainty()
                idx = _grade_index(horiz, self._maxHorizUnc)
                indices.append(idx)
                details["horizUnc"] = (horiz, GRADES[idx])
            except Exception:
                pass

            try:
                depUnc = ou.depthUncertainty()
                idx = _grade_index(depUnc, self._maxDepUnc)
                indices.append(idx)
                details["depUnc"] = (depUnc, GRADES[idx])
            except Exception:
                pass

        except Exception:
            pass

        if not indices:
            return None, {}

        return GRADES[max(indices)], details

    # ------------------------------------------------------------------
    # Output helpers
    # ------------------------------------------------------------------

    def _logGrade(self, originID, grade, details):
        detail_str = ", ".join(
            "%s=%s(%s)" % (k,
                           ("%.1f" % v[0] if isinstance(v[0], float) else str(v[0])),
                           v[1])
            for k, v in details.items()
        )
        seiscomp.logging.info(
            "scoriginquality: %s -> %s [%s]" % (originID, grade, detail_str)
        )

    def _buildCommentText(self, grade, details):
        labels = {
            "gap":      ("Az. Gap",      "°",   "%.1f"),
            "secGap":   ("Sec. Gap",     "°",   "%.1f"),
            "rms":      ("RMS",          " s",  "%.3f"),
            "stations": ("Stations",     "",    "%d"),
            "minDist":  ("Min. Dist",    "°",   "%.1f"),
            "GT":       ("GT Level",     "",    "%s"),
            "horizUnc": ("Horiz. Unc.",  " km", "%.1f"),
            "depUnc":   ("Depth Unc.",   " km", "%.1f"),
        }
        lines = [grade]
        for key in ("gap", "secGap", "rms", "stations", "minDist",
                    "horizUnc", "depUnc", "GT"):
            if key not in details:
                continue
            val, g = details[key]
            label, unit, fmt = labels[key]
            valstr = fmt % val
            lines.append("%s: %s%s \u2192 %s" % (label, valstr, unit, g))
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
