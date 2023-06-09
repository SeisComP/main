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
import seiscomp.client
import seiscomp.io
import seiscomp.math
import seiscomp.datamodel
import seiscomp.logging
import seiscomp.seismology

from seiscomp.datamodel import QMLTypeMapper


def time2str(time):
    """
    Convert a seiscomp.core.Time to a string
    """
    return time.toString("%Y-%m-%d %H:%M:%S.%f000000")[:23]


def lat2str(lat, enhanced=False):
    if enhanced:
        s = "%.5f " % abs(lat)
    else:
        s = "%.2f " % abs(lat)
    if lat >= 0:
        s += "N"
    else:
        s += "S"
    return s


def lon2str(lon, enhanced=False):
    if enhanced:
        s = "%.5f " % abs(lon)
    else:
        s = "%.2f " % abs(lon)
    if lon >= 0:
        s += "E"
    else:
        s += "W"
    return s


def stationCount(org, minArrivalWeight):
    count = 0
    for i in range(org.arrivalCount()):
        arr = org.arrival(i)
        #   if eight()> 0.5:
        wt = arrivalWeight(arr)
        if wt >= minArrivalWeight:
            count += 1

    return count


def uncertainty(quantity):
    # for convenience/readability: get uncertainty from a quantity
    try:
        err = 0.5 * (quantity.lowerUncertainty() + quantity.upperUncertainty())
    except ValueError:
        try:
            err = quantity.uncertainty()
        except ValueError:
            err = None

    return err


def arrivalWeight(arrival):
    # guess arrival weight if not available
    wt = 0
    try:
        wt = arrival.weight()
    except ValueError:
        if (
            arrival.timeUsed()
            or arrival.horizontalSlownessUsed()
            or arrival.backazimuthUsed()
        ):
            wt = 1

    return wt


def createKML(mode):
    if mode == "open":
        text = '<?xml version="1.0" encoding="UTF-8"?>'
        text += '<kml xmlns="http://www.opengis.net/kml/2.2">'
        text += "<Document><name>SeisComP event parameters</name>"
        text += "<name>SeisComP: earthquakes</name>"
        text += '<Style id="earthquake"><LabelStyle><scale>1</scale></LabelStyle><IconStyle>'
        text += "<Icon><href>http://maps.google.com/mapfiles/kml/paddle/wht-stars.png</href></Icon>"
        text += '<hotSpot x="0.5" y="0.0" xunits="fraction" yunits="fraction"/>'
        text += "</IconStyle></Style>"
    elif mode == "close":
        text = "</Document></kml>"
    else:
        return False

    return text


class Bulletin(object):
    def __init__(self, dbq, long=True):
        self._dbq = dbq
        self._long = long
        self._evt = None
        self.format = "autoloc1"
        self.enhanced = False
        self.polarities = False
        self.useEventAgencyID = False
        self.distInKM = False
        self.minDepthPhaseCount = 3
        self.minArrivalWeight = 0.5
        self.minStationMagnitudeWeight = 0.5

    def _getArrivalsSorted(self, org):
        # returns arrival list sorted by distance
        arrivals = [org.arrival(i) for i in range(org.arrivalCount())]
        return sorted(arrivals, key=lambda t: t.distance())

    def _getPicks(self, org):
        if self._dbq is None:
            return {}
        orid = org.publicID()
        pick = {}
        for obj in self._dbq.getPicks(orid):
            p = seiscomp.datamodel.Pick.Cast(obj)
            key = p.publicID()
            pick[key] = p
        return pick

    def _getAmplitudes(self, org):
        if self._dbq is None:
            return {}
        orid = org.publicID()
        ampl = {}
        for obj in self._dbq.getAmplitudesForOrigin(orid):
            amp = seiscomp.datamodel.Amplitude.Cast(obj)
            key = amp.publicID()
            ampl[key] = amp
        return ampl

    def _printOriginAutoloc3(self, org, extra=False):
        orid = org.publicID()

        arrivals = self._getArrivalsSorted(org)
        pick = self._getPicks(org)
        ampl = self._getAmplitudes(org)

        try:
            depthPhaseCount = org.quality().depthPhaseCount()
        except ValueError:
            depthPhaseCount = 0
            for arr in arrivals:
                wt = arrivalWeight(arr)
                pha = arr.phase().code()
                #  if (pha[0] in ["p","s"] and wt >= 0.5 ):
                if pha[0] in ["p", "s"] and wt >= self.minArrivalWeight:
                    depthPhaseCount += 1

        txt = ""
        evt = self._evt
        if not evt and self._dbq:
            evt = self._dbq.getEvent(orid)

        if evt:
            txt += "Event:\n"
            txt += "    Public ID              %s\n" % evt.publicID()
            if extra:
                txt += "    Preferred Origin ID    %s\n" % evt.preferredOriginID()
                txt += "    Preferred Magnitude ID %s\n" % evt.preferredMagnitudeID()
            try:
                evtType = evt.type()
                txt += (
                    "    Type                   %s\n"
                    % seiscomp.datamodel.EEventTypeNames.name(evtType)
                )
            except ValueError:
                seiscomp.logging.debug(
                    "%s: ignoring empty or unknown event type" % evt.publicID()
                )

            txt += "    Description\n"
            for i in range(evt.eventDescriptionCount()):
                evtd = evt.eventDescription(i)
                evtdtype = seiscomp.datamodel.EEventDescriptionTypeNames.name(
                    evtd.type()
                )
                txt += "      %s: %s" % (evtdtype, evtd.text())

            if extra:
                try:
                    txt += (
                        "\n    Creation time          %s\n"
                        % evt.creationInfo()
                        .creationTime()
                        .toString("%Y-%m-%d %H:%M:%S")
                    )
                except ValueError:
                    pass
            txt += "\n"
            preferredMagnitudeID = evt.preferredMagnitudeID()
        else:
            preferredMagnitudeID = ""

        tim = org.time().value()
        lat = org.latitude().value()
        lon = org.longitude().value()
        dep = org.depth().value()
        timerr = uncertainty(org.time())
        laterr = uncertainty(org.latitude())
        lonerr = uncertainty(org.longitude())
        deperr = uncertainty(org.depth())
        tstr = time2str(tim)

        originHeader = "Origin:\n"
        if evt:
            if org.publicID() != evt.preferredOriginID():
                originHeader = "Origin (NOT the preferred origin of this event):\n"

        txt += originHeader
        if extra:
            txt += "    Public ID              %s\n" % org.publicID()
        txt += "    Date                   %s\n" % tstr[:10]
        if timerr:
            if self.enhanced:
                txt += "    Time                   %s   +/- %8.3f s\n" % (
                    tstr[11:],
                    timerr,
                )
            else:
                txt += "    Time                   %s  +/- %6.1f s\n" % (
                    tstr[11:-2],
                    timerr,
                )
        else:
            if self.enhanced:
                txt += "    Time                   %s\n" % tstr[11:]
            else:
                txt += "    Time                   %s\n" % tstr[11:-2]

        if laterr:
            if self.enhanced:
                txt += "    Latitude              %10.5f deg  +/- %8.3f km\n" % (
                    lat,
                    laterr,
                )
            else:
                txt += "    Latitude              %7.2f deg  +/- %6.0f km\n" % (
                    lat,
                    laterr,
                )
        else:
            if self.enhanced:
                txt += "    Latitude              %10.5f deg\n" % lat
            else:
                txt += "    Latitude              %7.2f deg\n" % lat
        if lonerr:
            if self.enhanced:
                txt += "    Longitude             %10.5f deg  +/- %8.3f km\n" % (
                    lon,
                    lonerr,
                )
            else:
                txt += "    Longitude             %7.2f deg  +/- %6.0f km\n" % (
                    lon,
                    lonerr,
                )
        else:
            if self.enhanced:
                txt += "    Longitude             %10.5f deg\n" % lon
            else:
                txt += "    Longitude             %7.2f deg\n" % lon
        if self.enhanced:
            txt += "    Depth                %11.3f km" % dep
        else:
            txt += "    Depth                 %7.0f km" % dep
        if deperr is None:
            txt += "\n"
        elif deperr == 0:
            txt += "   (fixed)\n"
        else:
            if depthPhaseCount >= self.minDepthPhaseCount:
                if self.enhanced:
                    txt += "   +/- %8.3f km  (%d depth phases)\n" % (
                        deperr,
                        depthPhaseCount,
                    )
                else:
                    txt += "   +/- %4.0f km  (%d depth phases)\n" % (
                        deperr,
                        depthPhaseCount,
                    )
            else:
                if self.enhanced:
                    txt += "   +/- %8.3f km\n" % deperr
                else:
                    txt += "   +/- %4.0f km\n" % deperr

        agencyID = ""
        if self.useEventAgencyID:
            try:
                agencyID = evt.creationInfo().agencyID()
            except ValueError:
                pass
        else:
            try:
                agencyID = org.creationInfo().agencyID()
            except ValueError:
                pass

        txt += "    Agency                 %s\n" % agencyID
        if extra:
            try:
                authorID = org.creationInfo().author()
            except ValueError:
                authorID = "NOT SET"
            txt += "    Author                 %s\n" % authorID
        txt += "    Mode                   "
        try:
            txt += "%s\n" % seiscomp.datamodel.EEvaluationModeNames.name(
                org.evaluationMode()
            )
        except ValueError:
            txt += "NOT SET\n"
        txt += "    Status                 "
        try:
            txt += "%s\n" % seiscomp.datamodel.EEvaluationStatusNames.name(
                org.evaluationStatus()
            )
        except ValueError:
            txt += "NOT SET\n"

        if extra:
            txt += "    Creation time          "
            try:
                txt += "%s\n" % org.creationInfo().creationTime().toString(
                    "%Y-%m-%d %H:%M:%S"
                )
            except ValueError:
                txt += "NOT SET\n"

        try:
            if self.enhanced:
                txt += (
                    "    Residual RMS           %9.3f s\n"
                    % org.quality().standardError()
                )
            else:
                txt += (
                    "    Residual RMS           %6.2f s\n"
                    % org.quality().standardError()
                )
        except ValueError:
            pass

        try:
            if self.enhanced:
                txt += (
                    "    Azimuthal gap           %8.1f deg\n"
                    % org.quality().azimuthalGap()
                )
            else:
                txt += (
                    "    Azimuthal gap           %5.0f deg\n"
                    % org.quality().azimuthalGap()
                )
        except ValueError:
            pass

        txt += "\n"

        networkMagnitudeCount = org.magnitudeCount()
        networkMagnitudes = {}

        # Each station magnitude contributes to the network
        # magnitude of the same type.
        #
        # We save here the StationMagnitudeContribution objects
        # by publicID of the corresponding StationMagnitude object.
        stationMagnitudeContributions = {}

        tmptxt = txt
        txt = ""
        foundPrefMag = False
        for i in range(networkMagnitudeCount):
            mag = org.magnitude(i)
            val = mag.magnitude().value()
            typ = mag.type()
            networkMagnitudes[typ] = mag

            for k in range(mag.stationMagnitudeContributionCount()):
                smc = mag.stationMagnitudeContribution(k)
                smid = smc.stationMagnitudeID()
                stationMagnitudeContributions[smid] = smc

            err = uncertainty(mag.magnitude())
            if err is not None:
                err = "+/- %.2f" % err
            else:
                err = ""

            if mag.publicID() == preferredMagnitudeID:
                preferredMarker = "preferred"
                foundPrefMag = True
            else:
                preferredMarker = "         "
            if extra:
                try:
                    agencyID = mag.creationInfo().agencyID()
                except ValueError:
                    pass
            else:
                agencyID = ""
            txt += "    %-8s %5.2f %8s %3d %s  %s\n" % (
                typ,
                val,
                err,
                mag.stationCount(),
                preferredMarker,
                agencyID,
            )

        if not foundPrefMag and preferredMagnitudeID != "":
            mag = seiscomp.datamodel.Magnitude.Find(preferredMagnitudeID)
            if mag is None and self._dbq:
                o = self._dbq.loadObject(
                    seiscomp.datamodel.Magnitude.TypeInfo(), preferredMagnitudeID
                )
                mag = seiscomp.datamodel.Magnitude.Cast(o)

            if mag:
                val = mag.magnitude().value()
                typ = mag.type()
                networkMagnitudes[typ] = mag

                err = uncertainty(mag.magnitude())
                if err is not None:
                    err = "+/- %.2f" % err
                else:
                    err = ""

                preferredMarker = "preferred"
                if extra:
                    try:
                        agencyID = mag.creationInfo().agencyID()
                    except ValueError:
                        pass
                else:
                    agencyID = ""
                txt += "    %-8s %5.2f %8s %3d %s  %s\n" % (
                    typ,
                    val,
                    err,
                    mag.stationCount(),
                    preferredMarker,
                    agencyID,
                )

        txt = tmptxt + "%d Network magnitudes:\n" % networkMagnitudeCount + txt

        if not self._long:
            return txt

        lineFMT = "    %-5s %-2s  "
        if self.enhanced:
            lineFMT += "%9.3f" if self.distInKM else "%9.5f"
        else:
            lineFMT += "%5.0f" if self.distInKM else "%5.1f"
        lineFMT += " %s  %-7s %s %s %1s%1s %3.1f  "
        if self.polarities:
            lineFMT += "%s "
        lineFMT += "%-5s\n"

        dist_azi = {}
        lines = []

        for arr in arrivals:
            p = seiscomp.datamodel.Pick.Find(arr.pickID())
            if p is None:
                lines.append((180, "    ## missing pick %s\n" % arr.pickID()))
                continue

            if self.distInKM:
                dist = seiscomp.math.deg2km(arr.distance())
            else:
                dist = arr.distance()

            wfid = p.waveformID()
            net = wfid.networkCode()
            sta = wfid.stationCode()
            if self.enhanced:
                try:
                    azi = "%5.1f" % arr.azimuth()
                except ValueError:
                    azi = "  N/A"
                tstr = time2str(p.time().value())[11:]
                try:
                    res = "%7.3f" % arr.timeResidual()
                except ValueError:
                    res = "    N/A"
            else:
                try:
                    azi = "%3.0f" % arr.azimuth()
                except ValueError:
                    azi = "N/A"
                tstr = time2str(p.time().value())[11:-2]
                try:
                    res = "%5.1f" % arr.timeResidual()
                except ValueError:
                    res = "  N/A"

            dist_azi[net + "_" + sta] = (dist, azi)
            wt = arrivalWeight(arr)
            pha = arr.phase().code()
            flag = "X "[wt > 0.1]

            try:
                status = seiscomp.datamodel.EEvaluationModeNames.name(
                    p.evaluationMode()
                )[0].upper()
            except ValueError:
                status = "-"
            if self.polarities:
                try:
                    pol = seiscomp.datamodel.EPickPolarityNames.name(p.polarity())
                except ValueError:
                    pol = None
                if pol:
                    if pol == "positive":
                        pol = "u"
                    elif pol == "negative":
                        pol = "d"
                    elif pol == "undecidable":
                        pol = "x"
                    else:
                        pol = "."
                else:
                    pol = "."
                line = lineFMT % (
                    sta,
                    net,
                    dist,
                    azi,
                    pha,
                    tstr,
                    res,
                    status,
                    flag,
                    wt,
                    pol,
                    sta,
                )
            else:
                line = lineFMT % (
                    sta,
                    net,
                    dist,
                    azi,
                    pha,
                    tstr,
                    res,
                    status,
                    flag,
                    wt,
                    sta,
                )
            lines.append((dist, line))

        lines.sort()

        txt += "\n"
        txt += "%d Phase arrivals:\n" % org.arrivalCount()
        if self.enhanced:
            txt += (
                "    sta   net      dist   azi  phase   time             res     wt  "
            )
        else:
            txt += "    sta   net  dist azi  phase   time         res     wt  "
        if self.polarities:
            txt += "  "
        txt += "sta  \n"
        for dist, line in lines:
            txt += line
        txt += "\n"

        stationMagnitudeCount = org.stationMagnitudeCount()
        activeStationMagnitudeCount = 0
        stationMagnitudes = {}

        for i in range(stationMagnitudeCount):
            mag = org.stationMagnitude(i)
            typ = mag.type()
            if typ not in networkMagnitudes:
                continue
            if typ not in stationMagnitudes:
                stationMagnitudes[typ] = []

            # suppress unused station magnitudes
            smid = mag.publicID()
            if not smid in stationMagnitudeContributions:
                continue

            try:
                w = stationMagnitudeContributions[smid].weight()
            except ValueError:
                w = self.minStationMagnitudeWeight
            if w < self.minStationMagnitudeWeight:
                continue
            stationMagnitudes[typ].append(mag)
            activeStationMagnitudeCount += 1

        lineFMT = "    %-5s %-2s  "
        if self.enhanced:
            lineFMT += "%9.3f" if self.distInKM else "%9.5f"
        else:
            lineFMT += "%5.0f" if self.distInKM else "%5.1f"
        lineFMT += " %s  %-6s %5.2f %5.2f   %8s %4s\n"

        lines = []

        for typ in stationMagnitudes:
            for mag in stationMagnitudes[typ]:
                key = mag.amplitudeID()
                if key:
                    amp = seiscomp.datamodel.Amplitude.Find(key)
                    if amp is None and self._dbq:
                        seiscomp.logging.debug("missing station amplitude '%s'" % key)

                        # try to load amplitude from database
                        obj = self._dbq.loadObject(
                            seiscomp.datamodel.Amplitude.TypeInfo(), key
                        )
                        amp = seiscomp.datamodel.Amplitude.Cast(obj)
                else:
                    amp = None
                    # This is expected behaviour for some magnitudes like Me.

                p = ""
                a = "N/A"
                if amp:
                    try:
                        a = "%g" % amp.amplitude().value()
                    except ValueError:
                        a = "N/A"

                    if typ in ["mb", "Ms", "Ms(BB)"]:
                        try:
                            p = "%.2f" % amp.period().value()
                        except ValueError:
                            p = "N/A"
                    else:
                        p = ""

                wfid = mag.waveformID()
                net = wfid.networkCode()
                sta = wfid.stationCode()

                try:
                    dist, azi = dist_azi[net + "_" + sta]
                except KeyError:
                    dist, azi = 0, "  N/A" if self.enhanced else "N/A"

                val = mag.magnitude().value()
                res = val - networkMagnitudes[typ].magnitude().value()

                line = lineFMT % (sta, net, dist, azi, typ, val, res, a, p)
                lines.append((dist, line))

        lines.sort()

        if activeStationMagnitudeCount:
            txt += "%d Station magnitudes:\n" % activeStationMagnitudeCount
            if self.enhanced:
                txt += "    sta   net      dist   azi  type   value   res        amp  per\n"
            else:
                txt += "    sta   net  dist azi  type   value   res        amp  per\n"
            for dist, line in lines:
                txt += line
        else:
            txt += "No station magnitudes\n"

        return txt

    def _printOriginAutoloc1(self, org):
        evt = self._evt

        if not evt and self._dbq:
            evt = self._dbq.getEvent(org.publicID())

        if evt:
            evid = evt.publicID()
            pos = evid.find("#")  # XXX Hack!!!
            if pos != -1:
                evid = evid[:pos]
            prefMagID = evt.preferredMagnitudeID()
        else:
            evid = "..."
            prefMagID = ""

        txt = ""

        if self.enhanced:
            depth = org.depth().value()
            sTime = org.time().value().toString("%Y/%m/%d  %H:%M:%S.%f00")[:24]
        else:
            depth = int(org.depth().value() + 0.5)
            sTime = org.time().value().toString("%Y/%m/%d  %H:%M:%S.%f")[:22]

        tmp = {
            "evid": evid,
            "nsta": stationCount(org, self.minArrivalWeight),
            "time": sTime,
            "lat": lat2str(org.latitude().value(), self.enhanced),
            "lon": lon2str(org.longitude().value(), self.enhanced),
            "dep": depth,
            "reg": seiscomp.seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value()
            ),
            # changed to properly report location method. (Marco Olivieri 21/06/2010)
            "method": org.methodID(),
            "model": org.earthModelID(),
            # end (MO)
            "stat": "A",
        }

        try:
            if org.evaluationMode() == seiscomp.datamodel.MANUAL:
                tmp["stat"] = "M"
        except ValueError:
            pass

        # dummy default
        tmp["mtyp"] = "M"
        tmp["mval"] = 0.0

        foundMag = False
        networkMagnitudeCount = org.magnitudeCount()
        for i in range(networkMagnitudeCount):
            mag = org.magnitude(i)
            if mag.publicID() == prefMagID:
                tmp["mtyp"] = mag.type()
                tmp["mval"] = mag.magnitude().value()
                foundMag = True
                break

        if not foundMag and prefMagID != "":
            mag = seiscomp.datamodel.Magnitude.Find(prefMagID)
            if mag is None and self._dbq:
                o = self._dbq.loadObject(
                    seiscomp.datamodel.Magnitude.TypeInfo(), prefMagID
                )
                mag = seiscomp.datamodel.Magnitude.Cast(o)

            if mag:
                tmp["mtyp"] = mag.type()
                tmp["mval"] = mag.magnitude().value()

        # changed to properly report location method. (Marco Olivieri 21/06/2010)
        #        txt += """
        # Autoloc alert %(evid)s: determined by %(nsta)d stations, type %(stat)s
        #
        # LOCSAT solution (with start solution, %(nsta)d stations used, weight %(nsta)d):
        #
        #  %(reg)s  %(mtyp)s=%(mval).1f  %(time)s  %(lat)s  %(lon)s   %(dep)d km
        #
        #  Stat  Net   Date       Time          Amp    Per   Res  Dist  Az mb  ML  mB
        # """ % tmp
        txtFMT = """
  Alert %(evid)s: determined by %(nsta)d stations, type %(stat)s

 %(method)s solution with earthmodel %(model)s (with start solution, %(nsta)d stations used, weight %(nsta)d):

  %(reg)s  %(mtyp)s=%(mval).1f  %(time)s  %(lat)s  %(lon)s   %(dep)"""
        if self.enhanced:
            txtFMT += """.3f km

  Stat  Net Date      Time                 Amp  Per     Res      Dist    Az  mb  ML  mB
"""
        else:
            txtFMT += """d km

  Stat  Net Date      Time               Amp  Per   Res  Dist  Az  mb  ML  mB
"""
        txt += txtFMT % tmp

        # end (MO)
        arrivals = self._getArrivalsSorted(org)
        pick = self._getPicks(org)
        ampl = self._getAmplitudes(org)

        stationMagnitudeCount = org.stationMagnitudeCount()
        stationMagnitudes = {}
        for i in range(stationMagnitudeCount):
            mag = org.stationMagnitude(i)
            typ = mag.type()
            if typ == "MLv" or typ == "MLh":
                typ = "ML"
            if typ not in stationMagnitudes:
                stationMagnitudes[typ] = {}

            sta = mag.waveformID().stationCode()
            stationMagnitudes[typ][sta] = mag

        lineFMT = "  %-5s %-2s  %s  %10.1f %4.1f %s "
        if self.enhanced:
            lineFMT += "%9.3f" if self.distInKM else "%9.5f"
        else:
            lineFMT += "%5.0f" if self.distInKM else "%5.1f"
        lineFMT += " %s%s\n"

        for arr in arrivals:
            wt = arrivalWeight(arr)
            if wt < self.minArrivalWeight:
                continue

            if self.distInKM:
                dist = seiscomp.math.deg2km(arr.distance())
            else:
                dist = arr.distance()

            p = seiscomp.datamodel.Pick.Find(arr.pickID())
            if p is None:
                txt += "  ## missing pick %s\n" % arr.pickID()
                continue

            wfid = p.waveformID()
            net = wfid.networkCode()
            sta = wfid.stationCode()
            if self.enhanced:
                tstr = p.time().value().toString("%y/%m/%d  %H:%M:%S.%f00")[:22]
                try:
                    res = "%7.3f" % arr.timeResidual()
                except ValueError:
                    res = "    N/A"
                try:
                    azi = "%5.1f" % arr.azimuth()
                except ValueError:
                    azi = "  N/A"
            else:
                tstr = p.time().value().toString("%y/%m/%d  %H:%M:%S.%f")[:20]
                try:
                    res = "%5.1f" % arr.timeResidual()
                except ValueError:
                    res = "  N/A"
                try:
                    azi = "%3.0f" % arr.azimuth()
                except ValueError:
                    azi = "N/A"

            pha = arr.phase().code()
            mstr = ""
            amp = per = 0.0
            for typ in ["mb", "ML", "mB"]:
                mag = 0.0
                try:
                    m = stationMagnitudes[typ][sta]
                    mag = m.magnitude().value()
                    if typ == "mb":
                        ampid = m.amplitudeID()
                        a = seiscomp.datamodel.Amplitude.Find(ampid)
                        if a is None and self._dbq:
                            obj = self._dbq.loadObject(
                                seiscomp.datamodel.Amplitude.TypeInfo(), ampid
                            )
                            a = seiscomp.datamodel.Amplitude.Cast(obj)
                        if a:
                            per = a.period().value()
                            try:
                                amp = a.amplitude().value()
                            except ValueError:
                                amp = -1
                except KeyError:
                    pass
                mstr += " %3.1f" % mag

            txt += lineFMT % (sta, net, tstr, amp, per, res, dist, azi, mstr)

        if self.enhanced:
            txt += "\n RMS-ERR:         %.3f\n\n" % org.quality().standardError()
        else:
            txt += "\n RMS-ERR:         %.2f\n\n" % org.quality().standardError()

        if evt:
            try:
                if self.enhanced:
                    tm = (
                        evt.creationInfo()
                        .creationTime()
                        .toString("%Y/%m/%d %H:%M:%S.%f")
                    )
                else:
                    tm = evt.creationInfo().creationTime().toString("%Y/%m/%d %H:%M:%S")

                txt += " Event created:       %s\n" % tm
            except ValueError:
                pass

        try:
            if self.enhanced:
                tm = org.creationInfo().creationTime().toString("%Y/%m/%d %H:%M:%S.%f")
            else:
                tm = org.creationInfo().creationTime().toString("%Y/%m/%d %H:%M:%S")
            txt += " This origin created: %s\n" % tm
        except ValueError:
            pass

        return txt

    def _printOriginKML(self, org):
        txt = ""
        evt = self._evt
        if not evt and self._dbq:
            evt = self._dbq.getEvent(org.publicID())

        author = None
        agencyID = None
        eType = None
        prefMagID = None
        depth = None
        altitude = None
        sTime = None
        mode = None
        status = None
        if evt:
            evid = evt.publicID()
            pos = evid.find("#")  # XXX Hack!!!
            if pos != -1:
                evid = evid[:pos]

            try:
                prefMagID = evt.preferredMagnitudeID()
            except ValueError:
                pass

            try:
                author = org.creationInfo().author()
            except ValueError:
                pass

            try:
                mode = seiscomp.datamodel.EEvaluationModeNames.name(
                    org.evaluationMode()
                )
            except ValueError:
                pass

            try:
                status = seiscomp.datamodel.EEvaluationStatusNames.name(
                    org.evaluationStatus()
                )
            except ValueError:
                pass

            if self.useEventAgencyID:
                try:
                    agencyID = evt.creationInfo().agencyID()
                except ValueError:
                    pass
            else:
                try:
                    agencyID = org.creationInfo().agencyID()
                except ValueError:
                    pass

            try:
                eType = seiscomp.datamodel.EEventTypeNames.name(evt.type())
            except ValueError:
                pass
        else:
            evid = ""
            prefMagID = ""

        if evt:
            ID = evt.publicID()
        else:
            ID = org.publicID()

        if self.enhanced:
            latitude = "{:.3f}".format(org.latitude().value())
            longitude = "{:.3f}".format(org.longitude().value())
            depth = float("{:.3f}".format(org.depth().value()))
            sTime = org.time().value().toString("%FT%T.%f")[:23]
        else:
            latitude = "{:.1f}".format(org.latitude().value())
            longitude = "{:.1f}".format(org.longitude().value())
            depth = float("{:.0f}".format(org.depth().value()))
            sTime = org.time().value().toString("%FT%T.%f")[:21]

        if depth:
            altitude = -1.0 * float(depth)

        try:
            region = seiscomp.seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value()
            )
        except ValueError:
            region = ""

        mag = False
        mType = ""
        mVal = None
        networkMagnitudeCount = org.magnitudeCount()
        for i in range(networkMagnitudeCount):
            mag = org.magnitude(i)
            if mag.publicID() == prefMagID:
                mType = mag.type()
                mVal = "{:.1f}".format(mag.magnitude().value())
                break

        if not mag and prefMagID != "":
            mag = seiscomp.datamodel.Magnitude.Find(prefMagID)
            if mag is None and self._dbq:
                o = self._dbq.loadObject(
                    seiscomp.datamodel.Magnitude.TypeInfo(), prefMagID
                )
                mag = seiscomp.datamodel.Magnitude.Cast(o)

            if mag:
                mType = mag.type()
                mVal = "{:.1f}".format(mag.magnitude().value())

        txt = "<Placemark>"
        txt += "<ExtendedData>"
        if eType:
            txt += f'<Data name="event type"><value>{eType}</value></Data>'
        txt += f'<Data name="time [UTC]"><value>{sTime}</value></Data>'
        if mVal:
            txt += f'<Data name="magnitude ({mType})"><value>{mVal}</value></Data>'
        else:
            txt += '<Data name="magnitude"><value>None</value></Data>'
        if len(region) > 0:
            txt += f'<Data name="region"><value>{region}</value></Data>'
        txt += f'<Data name="latitude [degree North]"><value>{latitude}</value></Data>'
        txt += f'<Data name="longitude [degree East]"><value>{longitude}</value></Data>'
        txt += f'<Data name="depth [km]"><value>{depth}</value></Data>'
        if agencyID:
            txt += f'<Data name="agency"><value>{agencyID}</value></Data>'
        if author:
            txt += f'<Data name="author"><value>{author}</value></Data>'
        if mode:
            txt += f'<Data name="mode"><value>{mode}</value></Data>'
        if status:
            txt += f'<Data name="status"><value>{status}</value></Data>'
        txt += "</ExtendedData>"
        txt += f"<name>{ID}</name>"
        txt += "<styleUrl>#earthquake</styleUrl>"
        # use icon scaling like in SC but devide by 10
        if mVal:
            size = 4.9 * (float(mVal) - 1.2) / 10.0
        else:
            size = 1
        # scale symbolColor with depth
        # global default: 0:FF0000,50:ffA500,100:FFFF00,250:00FF00,600:0000FF
        if depth < 50:
            symbolColor = "ff0000ff"
        elif depth >= 50 and depth < 100:
            symbolColor = "ff00a5ff"
        elif depth >= 100 and depth < 250:
            symbolColor = "ff00ffff"
        elif depth >= 250 and depth < 600:
            symbolColor = "ff00ff00"
        else:
            symbolColor = "ffff0000"
        txt += f"<Style><IconStyle><scale>{size}</scale><color>{symbolColor}</color></IconStyle></Style>"
        txt += f"<Point><coordinates>{longitude},{latitude},{altitude}</coordinates></Point>"
        txt += f"<TimeStamp>{sTime}</TimeStamp>"
        txt += "</Placemark>"

        return txt

    def _printOriginFDSN(self, org):
        evt = self._evt

        if not evt and self._dbq:
            evt = self._dbq.getEvent(org.publicID())

        author, agencyID, eType, prefMagID = "", "", "", ""
        if evt:
            evid = evt.publicID()
            pos = evid.find("#")  # XXX Hack!!!
            if pos != -1:
                evid = evid[:pos]

            try:
                prefMagID = evt.preferredMagnitudeID()
            except ValueError:
                pass

            try:
                author = org.creationInfo().author()
            except ValueError:
                pass

            if self.useEventAgencyID:
                try:
                    agencyID = evt.creationInfo().agencyID()
                except ValueError:
                    pass
            else:
                try:
                    agencyID = org.creationInfo().agencyID()
                except ValueError:
                    pass

            try:
                eType = QMLTypeMapper.EventTypeToString(evt.type())
            except ValueError:
                pass
        else:
            evid = ""
            prefMagID = ""

        if self.enhanced:
            depth = "{:.3f}".format(org.depth().value())
            sTime = org.time().value().toString("%FT%T.%f")[:26]
        else:
            depth = "{:.0f}".format(org.depth().value())
            sTime = org.time().value().toString("%FT%T.%f")[:23]

        lat = lat2str(org.latitude().value(), self.enhanced)
        lon = lon2str(org.longitude().value(), self.enhanced)

        try:
            region = seiscomp.seismology.Regions.getRegionName(
                org.latitude().value(), org.longitude().value()
            )
        except ValueError:
            region = ""

        foundMag = False
        mType, mVal, mAuthor = "", "", ""
        networkMagnitudeCount = org.magnitudeCount()
        for i in range(networkMagnitudeCount):
            mag = org.magnitude(i)
            if mag.publicID() == prefMagID:
                mType = mag.type()
                mVal = "{:.1f}".format(mag.magnitude().value())
                foundMag = True
                break

        if not foundMag and prefMagID != "":
            mag = seiscomp.datamodel.Magnitude.Find(prefMagID)
            if mag is None and self._dbq:
                o = self._dbq.loadObject(
                    seiscomp.datamodel.Magnitude.TypeInfo(), prefMagID
                )
                mag = seiscomp.datamodel.Magnitude.Cast(o)

            if mag:
                mType = mag.type()
                mVal = "{:.1f}".format(mag.magnitude().value())
                try:
                    mAuthor = mag.creationInfo().author()
                except ValueError:
                    pass

        txt = "%s|%s|%s|%s|%s|%s||%s|%s|%s|%s|%s|%s|%s\n" % (
            evid,
            sTime,
            lat,
            lon,
            depth,
            author,
            agencyID,
            evid,
            mType,
            mVal,
            mAuthor,
            region,
            eType,
        )

        return txt

    def printOrigin(self, origin):
        org = None
        if isinstance(origin, seiscomp.datamodel.Origin):
            org = origin
        elif isinstance(origin, str):
            if self._dbq:
                org = self._dbq.loadObject(seiscomp.datamodel.Origin.TypeInfo(), origin)
                org = seiscomp.datamodel.Origin.Cast(org)
            if not org:
                raise KeyError("Unknown origin " + origin)
        else:
            raise TypeError("illegal type for origin")

        if self.format == "fdsnws":
            return self._printOriginFDSN(org)
        elif self.format == "autoloc1":
            return self._printOriginAutoloc1(org)
        elif self.format == "autoloc3":
            return self._printOriginAutoloc3(org, extra=False)
        elif self.format == "autoloc3extra":
            return self._printOriginAutoloc3(org, extra=True)
        if self.format == "kml":
            return self._printOriginKML(org)
        else:
            pass

    def printEvent(self, event):
        try:
            evt = None
            if isinstance(event, seiscomp.datamodel.Event):
                self._evt = event
                org = seiscomp.datamodel.Origin.Find(event.preferredOriginID())
                if not org:
                    org = event.preferredOriginID()
                return self.printOrigin(org)
            elif isinstance(event, str):
                if self._dbq:
                    evt = self._dbq.loadObject(
                        seiscomp.datamodel.Event.TypeInfo(), event
                    )
                    evt = seiscomp.datamodel.Event.Cast(evt)
                    self._evt = evt
                if evt is None:
                    raise KeyError("unknown event " + event)
                return self.printOrigin(evt.preferredOriginID())
            else:
                raise TypeError("illegal type for event")
        finally:
            self._evt = None


class BulletinApp(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, True)
        self.setDaemonEnabled(False)
        self.setLoggingToStdErr(True)
        self.setLoadRegionsEnabled(True)

        self.format = "autoloc1"
        self.inputFile = None

    def createCommandLineDescription(self):
        try:
            self.commandline().addGroup("Input")
            self.commandline().addStringOption(
                "Input",
                "format,f",
                "Input format to use (xml [default], zxml (zipped xml), binary).",
            )
            self.commandline().addStringOption(
                "Input", "input,i", "Input file, default: stdin."
            )

            self.commandline().addGroup("Dump")
            self.commandline().addStringOption(
                "Dump",
                "event,E",
                "ID of event(s) to dump. Separate multiple IDs by comma.",
            )
            self.commandline().addStringOption(
                "Dump",
                "origin,O",
                "ID of origin(s) to dump. Separate multiple IDs by comma.",
            )
            self.commandline().addOption(
                "Dump",
                "event-agency-id",
                "Use agency ID information from event instead of preferred origin.",
            )
            self.commandline().addOption(
                "Dump",
                "first-only",
                "Dump only the first event/origin. "
                "Expects input from file or stdin.",
            )
            self.commandline().addOption(
                "Dump", "polarities,p", "Dump onset polarities."
            )
            self.commandline().addStringOption(
                "Dump", "weight,w", "Weight threshold for printed and counted picks."
            )

            self.commandline().addGroup("Output")
            self.commandline().addOption("Output", "autoloc1,1", "Format: autoloc1.")
            self.commandline().addOption("Output", "autoloc3,3", "Format: autoloc3.")
            self.commandline().addOption(
                "Output",
                "fdsnws,4",
                "Format: FDSNWS event text, e.g., for generating catalogs.",
            )
            self.commandline().addOption(
                "Output",
                "kml,5",
                "Format: KML, GIS file format.",
            )
            self.commandline().addOption(
                "Output",
                "enhanced,e",
                "Enhanced output precision for local earthquakes.",
            )
            self.commandline().addOption(
                "Output", "dist-in-km,k", "Print distances in km instead of degree."
            )
            self.commandline().addStringOption(
                "Output",
                "output,o",
                "Name of output file. If not given, all event parameters are printed to stdout.",
            )
            self.commandline().addOption(
                "Output", "extra,x", "Extra detailed autoloc3 format."
            )
        except RuntimeError:
            seiscomp.logging.warning("caught unexpected error %s" % sys.exc_info())

        return True

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        try:
            self.inputFile = self.commandline().optionString("input")
        except RuntimeError:
            self.inputFile = None

        if self.inputFile:
            self.setDatabaseEnabled(False, False)

        if not self.commandline().hasOption(
            "event"
        ) and not self.commandline().hasOption("origin"):
            self.setDatabaseEnabled(False, False)

        return True

    def printUsage(self):
        print(
            """Usage:
  scbulletin [options]

Generate bulletins from events or origins in various formats: autoloc1, autoloc3, fdsnws, kml."""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            """Examples:
Create a bulletin from one event in the seiscomp database
  scbulletin -d mysql://sysop:sysop@localhost/seiscomp -E gempa2012abcd

Create a bulletin from event parameters in XML
  scbulletin -i gempa2012abcd.xml
"""
        )
        return True

    def run(self):
        evid = None
        orid = None
        mw = None
        inputFile = None
        txt = None
        outputFile = None

        try:
            evid = self.commandline().optionString("event")
        except RuntimeError:
            pass

        try:
            orid = self.commandline().optionString("origin")
        except RuntimeError:
            pass

        if evid != "" or orid != "" or not self.inputFile:
            dbq = seiscomp.datamodel.DatabaseQuery(self.database())
        else:
            dbq = None

        bulletin = Bulletin(dbq)
        bulletin.format = "autoloc1"

        try:
            mw = self.commandline().optionString("weight")
        except RuntimeError:
            pass

        if mw != "" and mw is not None:
            bulletin.minArrivalWeight = float(mw)

        if self.commandline().hasOption("autoloc1"):
            bulletin.format = "autoloc1"
        elif self.commandline().hasOption("autoloc3"):
            if self.commandline().hasOption("extra"):
                bulletin.format = "autoloc3extra"
            else:
                bulletin.format = "autoloc3"

        if self.commandline().hasOption("fdsnws"):
            bulletin.format = "fdsnws"

        if self.commandline().hasOption("kml"):
            bulletin.format = "kml"

        if self.commandline().hasOption("enhanced"):
            bulletin.enhanced = True

        if self.commandline().hasOption("polarities"):
            bulletin.polarities = True

        if self.commandline().hasOption("event-agency-id"):
            bulletin.useEventAgencyID = True

        if self.commandline().hasOption("dist-in-km"):
            bulletin.distInKM = True

        try:
            outputFile = self.commandline().optionString("output")
        except RuntimeError:
            pass

        inputFile = self.inputFile

        if not self.inputFile:
            txt = ""
            try:
                if evid:
                    for ev in evid.split(","):
                        try:
                            txt += bulletin.printEvent(ev)
                        except ValueError:
                            seiscomp.logging.error("Unknown event '%s'" % ev)
                elif orid:
                    for org in orid.split(","):
                        try:
                            txt += bulletin.printOrigin(org)
                        except ValueError:
                            seiscomp.logging.error("Unknown origin '%s'" % org)
                else:
                    inputFile = "-"
                    print("Expecting input in XML from stdin", file=sys.stderr)

            except Exception as exc:
                print("ERROR: {}".format(exc), file=sys.stderr)
                # return False
        else:
            inputFile = self.inputFile

        if inputFile:
            inputFormat = "xml"
            try:
                try:
                    inputFormat = self.commandline().optionString("format")
                except RuntimeError:
                    pass

                if inputFormat == "xml":
                    ar = seiscomp.io.XMLArchive()
                elif inputFormat == "zxml":
                    ar = seiscomp.io.XMLArchive()
                    ar.setCompression(True)
                elif inputFormat == "binary":
                    ar = seiscomp.io.BinaryArchive()
                else:
                    raise TypeError("unknown input format: " + inputFormat)

                if not ar.open(inputFile):
                    raise IOError("Unable to open input file " + inputFile)

                obj = ar.readObject()
                if obj is None:
                    raise IOError("Invalid format in " + inputFile)

                ep = seiscomp.datamodel.EventParameters.Cast(obj)
                if ep is None:
                    raise TypeError("No event parameters found in " + inputFile)

                if ep.eventCount() <= 0:
                    if ep.originCount() <= 0:
                        raise TypeError(
                            "No origin and no event in event "
                            "parameters found in " + inputFile
                        )

                    if self.commandline().hasOption("first-only"):
                        org = ep.origin(0)
                        txt = bulletin.printOrigin(org)
                    else:
                        txt = ""
                        for i in range(ep.originCount()):
                            org = ep.origin(i)
                            if orid and org.publicID() not in orid:
                                seiscomp.logging.error(
                                    "%s: Skipping origin with ID %s"
                                    % (inputFile, org.publicID())
                                )
                                continue
                            txt += bulletin.printOrigin(org)
                else:
                    if self.commandline().hasOption("first-only"):
                        ev = ep.event(0)
                        if ev is None:
                            raise TypeError("Invalid event in " + inputFile)

                        try:
                            txt = bulletin.printEvent(ev)
                        except KeyError:
                            raise TypeError("Unknown event")
                    elif orid:
                        txt = ""
                        for oid in orid.split(","):
                            org = ep.findOrigin(oid)
                            if org:
                                txt += bulletin.printOrigin(org)
                            else:
                                seiscomp.logging.error(
                                    "%s: Skipping origin with ID %s" % (inputFile, oid)
                                )
                    else:
                        txt = ""
                        for i in range(ep.eventCount()):
                            ev = ep.event(i)
                            if evid and ev.publicID() not in evid:
                                seiscomp.logging.error(
                                    "%s: Skipping event with ID %s"
                                    % (inputFile, ev.publicID())
                                )
                                continue

                            try:
                                txt += bulletin.printEvent(ev)
                            except KeyError:
                                raise TypeError("Unknown event")

            except Exception as exc:
                seiscomp.logging.error("%s" % str(exc))
                return False

        if outputFile:
            print(f"Output data to file: {outputFile}", file=sys.stderr)
            try:
                out = open(outputFile, "w")
            except Exception:
                print("Cannot create output file {outputFile}", file=sys.stderr)
                return -1
        else:
            out = sys.stdout

        if bulletin.format == "fdsnws":
            print(
                "#EventID|Time|Latitude|Longitude|Depth/km|Author|"
                "Catalog|Contributor|ContributorID|MagType|Magnitude|"
                "MagAuthor|EventLocationName|EventType",
                file=out,
            )

        if bulletin.format == "kml":
            text = createKML("open")
            if text:
                print(f"{text}", file=out)
            else:
                return False

        if txt:
            print(f"{txt}", file=out)

        if bulletin.format == "kml":
            text = createKML("close")
            if text:
                print(f"{text}", file=out)
            else:
                return False

        return True


def main():
    app = BulletinApp(len(sys.argv), sys.argv)
    return app()


if __name__ == "__main__":
    sys.exit(main())
