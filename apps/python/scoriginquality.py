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
# Subscribes to LOCATION messages, reads OriginQuality fields from each
# incoming Origin, assigns a grade (A/B/C/D) using a worst-of rule, and
# writes the grade back as an origin Comment with a configurable ID.
#
# Grade thresholds (all configurable, defaults follow ISC/ANSS conventions):
#
#   Grade | maxGap | maxSecGap | maxRMS | minStations | maxMinDist
#   ------+--------+-----------+--------+-------------+-----------
#     A   |   90   |    135    |  0.15  |     10      |    30
#     B   |  135   |    180    |  0.30  |      6      |    60
#     C   |  180   |    210    |  0.50  |      4      |    90
#     D   |  360   |    360    |  inf   |      1      |   180
#
# Anything worse than D gets grade D.
############################################################################

import sys
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.logging


GRADES = ("A", "B", "C", "D")


def _grade_index(value, thresholds, lower_is_worse=True):
    """Return 0-3 index for A-D given sorted thresholds (A→B→C→D direction)."""
    for i, threshold in enumerate(thresholds):
        if lower_is_worse:
            if value <= threshold:
                return i
        else:
            if value >= threshold:
                return i
    return 3  # D


class OriginQualityApp(seiscomp.client.Application):

    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(False, False)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)
        self.addMessagingSubscription("LOCATION")

        self.setAutoApplyNotifierEnabled(True)
        self.setInterpretNotifierEnabled(True)

        # Comment ID written on the origin
        self._commentID = "quality"

        # Thresholds — list index 0=A, 1=B, 2=C, 3=D
        self._maxGap       = [90.0,  135.0, 180.0, 360.0]
        self._maxSecGap    = [135.0, 180.0, 210.0, 360.0]
        self._maxRMS       = [0.15,  0.30,  0.50,  1e9]
        self._minStations  = [10,    6,     4,     1]
        self._maxMinDist   = [30.0,  60.0,  90.0,  180.0]

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def createCommandLineDescription(self):
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

        try:
            self._maxGap[0] = self.configGetDouble("quality.A.maxGap")
        except Exception:
            pass
        try:
            self._maxGap[1] = self.configGetDouble("quality.B.maxGap")
        except Exception:
            pass
        try:
            self._maxGap[2] = self.configGetDouble("quality.C.maxGap")
        except Exception:
            pass

        try:
            self._maxSecGap[0] = self.configGetDouble("quality.A.maxSecondaryGap")
        except Exception:
            pass
        try:
            self._maxSecGap[1] = self.configGetDouble("quality.B.maxSecondaryGap")
        except Exception:
            pass
        try:
            self._maxSecGap[2] = self.configGetDouble("quality.C.maxSecondaryGap")
        except Exception:
            pass

        try:
            self._maxRMS[0] = self.configGetDouble("quality.A.maxRMS")
        except Exception:
            pass
        try:
            self._maxRMS[1] = self.configGetDouble("quality.B.maxRMS")
        except Exception:
            pass
        try:
            self._maxRMS[2] = self.configGetDouble("quality.C.maxRMS")
        except Exception:
            pass

        try:
            self._minStations[0] = self.configGetInt("quality.A.minStations")
        except Exception:
            pass
        try:
            self._minStations[1] = self.configGetInt("quality.B.minStations")
        except Exception:
            pass
        try:
            self._minStations[2] = self.configGetInt("quality.C.minStations")
        except Exception:
            pass

        try:
            self._maxMinDist[0] = self.configGetDouble("quality.A.maxMinDist")
        except Exception:
            pass
        try:
            self._maxMinDist[1] = self.configGetDouble("quality.B.maxMinDist")
        except Exception:
            pass
        try:
            self._maxMinDist[2] = self.configGetDouble("quality.C.maxMinDist")
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

        seiscomp.logging.info(
            "scoriginquality: writing grade to comment '%s'" % self._commentID
        )
        return True

    # ------------------------------------------------------------------
    # Messaging handlers
    # ------------------------------------------------------------------

    def addObject(self, parentID, obj):
        origin = seiscomp.datamodel.Origin.Cast(obj)
        if origin:
            self._processOrigin(parentID, origin)

    def updateObject(self, parentID, obj):
        origin = seiscomp.datamodel.Origin.Cast(obj)
        if origin:
            self._processOrigin(parentID, origin)

    # ------------------------------------------------------------------
    # Core grading logic
    # ------------------------------------------------------------------

    def _computeGrade(self, origin):
        """Return (grade_str, details_dict) for the given origin."""
        try:
            oq = origin.quality()
        except Exception:
            return None, {}

        indices = []
        details = {}

        # Azimuthal gap (primary)
        try:
            gap = oq.azimuthalGap()
            idx = _grade_index(gap, self._maxGap)
            indices.append(idx)
            details["gap"] = (gap, GRADES[idx])
        except Exception:
            pass

        # Secondary azimuthal gap
        try:
            sgap = oq.secondaryAzimuthalGap()
            idx = _grade_index(sgap, self._maxSecGap)
            indices.append(idx)
            details["secGap"] = (sgap, GRADES[idx])
        except Exception:
            pass

        # RMS / standard error
        try:
            rms = oq.standardError()
            idx = _grade_index(rms, self._maxRMS)
            indices.append(idx)
            details["rms"] = (rms, GRADES[idx])
        except Exception:
            pass

        # Used station count (higher is better)
        try:
            nsta = oq.usedStationCount()
            idx = _grade_index_stations(nsta, self._minStations)
            indices.append(idx)
            details["stations"] = (nsta, GRADES[idx])
        except Exception:
            pass

        # Minimum distance
        try:
            minDist = oq.minimumDistance()
            idx = _grade_index(minDist, self._maxMinDist)
            indices.append(idx)
            details["minDist"] = (minDist, GRADES[idx])
        except Exception:
            pass

        if not indices:
            return None, {}

        worst = max(indices)
        return GRADES[worst], details

    def _processOrigin(self, parentID, origin):
        grade, details = self._computeGrade(origin)
        if grade is None:
            seiscomp.logging.warning(
                "scoriginquality: %s has no usable OriginQuality — skipping"
                % origin.publicID()
            )
            return

        detail_str = ", ".join(
            "%s=%.3g(%s)" % (k, v[0], v[1]) for k, v in details.items()
        )
        seiscomp.logging.info(
            "scoriginquality: %s -> %s [%s]"
            % (origin.publicID(), grade, detail_str)
        )

        # Build or update the comment
        comment = seiscomp.datamodel.Comment()
        comment.setId(self._commentID)
        comment.setText(grade)
        ci = seiscomp.datamodel.CreationInfo()
        ci.setAgencyID(self.agencyID())
        ci.setAuthor("scoriginquality")
        ci.setCreationTime(seiscomp.core.Time.GMT())
        comment.setCreationInfo(ci)

        # Send as notifier on LOCATION group
        op = seiscomp.datamodel.OP_ADD

        # Check if origin already has this comment; use OP_UPDATE if so
        for i in range(origin.commentCount()):
            try:
                if origin.comment(i).id() == self._commentID:
                    op = seiscomp.datamodel.OP_UPDATE
                    break
            except Exception:
                pass

        msg = seiscomp.datamodel.NotifierMessage()
        n = seiscomp.datamodel.Notifier(origin.publicID(), op, comment)
        msg.attach(n)

        if not self.connection().send("LOCATION", msg):
            seiscomp.logging.error(
                "scoriginquality: failed to send grade for %s"
                % origin.publicID()
            )


def _grade_index_stations(nsta, min_thresholds):
    """Grade stations: A if nsta>=min_thresholds[0], B if >=min_thresholds[1], etc."""
    for i, threshold in enumerate(min_thresholds):
        if nsta >= threshold:
            return i
    return 3


def main():
    app = OriginQualityApp(len(sys.argv), sys.argv)
    return app()


if __name__ == "__main__":
    sys.exit(main())
