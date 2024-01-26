###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# FDSNStation -- Implements the fdsnws-availability Web service, see
#   http://www.fdsn.org/webservices/
#
# Feature notes:
#   - additional request parameters:
#     - excludetoolarge:       boolean, default: true
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

from functools import cmp_to_key

from collections import OrderedDict

from twisted.cred import portal
from twisted.internet.threads import deferToThread
from twisted.web import http, resource, server

from zope.interface import implementer

from seiscomp import datamodel, io, logging
from seiscomp.client import Application
from seiscomp.core import Time

from .http import BaseResource
from .request import RequestOptions

from . import utils


DBMaxUInt = 18446744073709551615  # 2^64 - 1
VERSION = "1.0.1"


###############################################################################
class _AvailabilityRequestOptions(RequestOptions):
    # merge options
    VMergeSampleRate = "samplerate"
    VMergeQuality = "quality"
    VMergeOverlap = "overlap"
    VMerge = [VMergeSampleRate, VMergeQuality]  # overlap option only available
    # in query method

    # orderby options
    VOrderByNSLC = "nslc_time_quality_samplerate"
    VOrderByCount = "timespancount"
    VOrderByCountDesc = "timespancount_desc"
    VOrderByUpdate = "latestupdate"
    VOrderByUpdateDesc = "latestupdate_desc"
    VOrderBy = [
        VOrderByNSLC,
        VOrderByCount,
        VOrderByCountDesc,
        VOrderByUpdate,
        VOrderByUpdateDesc,
    ]

    # format options
    VFormatText = "text"
    VFormatGeoCSV = "geocsv"
    VFormatJSON = "json"
    VFormatRequest = "request"
    OutputFormats = [VFormatText, VFormatGeoCSV, VFormatJSON, VFormatRequest]

    # request parameters
    PQuality = ["quality"]
    PMerge = ["merge"]
    POrderBy = ["orderby"]
    PLimit = ["limit"]
    PIncludeRestricted = ["includerestricted"]

    POSTParams = (
        RequestOptions.POSTParams
        + PQuality
        + PMerge
        + POrderBy
        + PLimit
        + PIncludeRestricted
    )
    GETParams = RequestOptions.GETParams + POSTParams

    # --------------------------------------------------------------------------
    def __init__(self):
        super().__init__()

        self.service = "availability-base"
        self.quality = None
        self.mergeSampleRate = None
        self.mergeQuality = None
        self.mergeOverlap = None
        self.orderBy = self.VOrderBy[0]
        self.limit = None
        self.includeRestricted = None
        self.showLatestUpdate = None

    # --------------------------------------------------------------------------
    def parse(self):
        self.parseTime()
        self.parseChannel()
        self.parseOutput()

        # quality: D, M, Q, R, * (optional)
        for v in self.getListValues(self.PQuality):
            if len(v) == 1:
                if v[0] == "*":
                    self.quality = None
                    break
                if v[0].isupper():
                    if self.quality is None:
                        self.quality = [v]
                    else:
                        self.quality.append(v)
                    continue
            self.raiseValueError(self.PQuality[0])

        # merge (optional)
        for v in self.getListValues(self.PMerge, True):
            if v not in self.VMerge:
                self.raiseValueError(self.PMerge[0])

            if v == self.VMergeSampleRate:
                self.mergeSampleRate = True
            elif v == self.VMergeQuality:
                self.mergeQuality = True
            elif v == self.VMergeOverlap:
                self.mergeOverlap = True

        # orderby (optional)
        key, value = self.getFirstValue(self.POrderBy)
        if value is not None:
            if value in self.VOrderBy:
                self.orderBy = value
            else:
                self.raiseValueError(key)

        # limit (optional)
        self.limit = self.parseInt(self.PLimit, 1, DBMaxUInt)

        # includeRestricted (optional)
        self.includeRestricted = self.parseBool(self.PIncludeRestricted)

    # --------------------------------------------------------------------------
    def extentIter(self, dac, user=None, access=None):
        # tupel: extent, oid, restricted
        for e in dac.extentsSorted():
            ext = e[0]
            restricted = e[2]
            wid = ext.waveformID()
            net = wid.networkCode()
            sta = wid.stationCode()
            loc = wid.locationCode()
            cha = wid.channelCode()

            if restricted:
                if not user:
                    continue

                if access:
                    startTime = ext.start()
                    endTime = ext.end()
                    if self.time:
                        if self.time.start() and self.time.start() > startTime:
                            startTime = self.time.start()
                        if self.time.end() and self.time.end() < ext.end():
                            endTime = self.time.end()

                    if not access.authorize(
                        user, net, sta, loc, cha, startTime, endTime
                    ):
                        continue

            for ro in self.streams:
                if ro.channel:
                    if (
                        not ro.channel.matchNet(net)
                        or not ro.channel.matchSta(sta)
                        or not ro.channel.matchLoc(loc)
                        or not ro.channel.matchCha(cha)
                    ):
                        continue

                if ro.time and not ro.time.match(ext.start(), ext.end()):
                    continue

                if not self.includeRestricted and restricted:
                    continue

                yield e


###############################################################################
class _AvailabilityExtentRequestOptions(_AvailabilityRequestOptions):
    # --------------------------------------------------------------------------
    def __init__(self):
        super().__init__()
        self.service = "availability-extent"

        self.showLatestUpdate = True

    # --------------------------------------------------------------------------
    def attributeExtentIter(self, ext):
        for i in range(ext.dataAttributeExtentCount()):
            attExt = ext.dataAttributeExtent(i)

            if self.time and not self.time.match(attExt.start(), attExt.end()):
                continue

            if self.quality and attExt.quality() not in self.quality:
                continue

            yield attExt


###############################################################################
class _AvailabilityQueryRequestOptions(_AvailabilityRequestOptions):
    # additional merge options
    VMerge = _AvailabilityRequestOptions.VMerge + [
        _AvailabilityRequestOptions.VMergeOverlap
    ]

    # show options
    VShowLatestUpdate = "latestupdate"
    VShow = [VShowLatestUpdate]

    # additional query specific request parameters
    PMergeGaps = ["mergegaps"]
    PShow = ["show"]
    PExcludeTooLarge = ["excludetoolarge"]

    POSTParams = (
        _AvailabilityRequestOptions.POSTParams + PMergeGaps + PShow + PExcludeTooLarge
    )
    GETParams = (
        _AvailabilityRequestOptions.GETParams + PMergeGaps + PShow + PExcludeTooLarge
    )

    # --------------------------------------------------------------------------
    def __init__(self):
        super().__init__()
        self.service = "availability-query"

        self.orderBy = None
        self.mergeGaps = None
        self.excludeTooLarge = None

    # --------------------------------------------------------------------------
    def parse(self):
        _AvailabilityRequestOptions.parse(self)

        # merge gaps threshold (optional)
        self.mergeGaps = self.parseFloat(self.PMergeGaps, 0)

        # show (optional)
        for v in self.getListValues(self.PShow, True):
            if v not in self.VShow:
                self.raiseValueError(v)
            if v == self.VShowLatestUpdate:
                self.showLatestUpdate = True

        # exclude to large (optional)
        self.excludeTooLarge = self.parseBool(self.PExcludeTooLarge)
        if self.excludeTooLarge is None:
            self.excludeTooLarge = True

        if not self.channel:
            raise ValueError("Request contains no selections")

        if self.orderBy:
            raise ValueError("orderby not supported for query request")


###############################################################################
class _Availability(BaseResource):
    isLeaf = True

    # --------------------------------------------------------------------------
    def __init__(self, access=None, user=None):
        super().__init__(VERSION)

        self.access = access
        self.user = user

    # --------------------------------------------------------------------------
    def render_OPTIONS(self, req):
        req.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        req.setHeader(
            "Access-Control-Allow-Headers",
            "Accept, Content-Type, X-Requested-With, Origin",
        )
        req.setHeader("Content-Type", "text/plain")
        return ""

    # --------------------------------------------------------------------------
    def render_GET(self, req):
        # Parse and validate GET parameters
        ro = self._createRequestOptions()
        try:
            ro.parseGET(req.args)
            ro.parse()

            # the GET operation supports exactly one stream filter
            ro.streams.append(ro)
        except ValueError as e:
            logging.warning(str(e))
            return self.renderErrorPage(req, http.BAD_REQUEST, str(e), ro)

        return self._prepareRequest(req, ro)

    # --------------------------------------------------------------------------
    def render_POST(self, req):
        # Parse and validate POST parameters
        ro = self._createRequestOptions()
        try:
            ro.parsePOST(req.content)
            ro.parse()
        except ValueError as e:
            logging.warning(str(e))
            return self.renderErrorPage(req, http.BAD_REQUEST, str(e), ro)

        return self._prepareRequest(req, ro)

    # --------------------------------------------------------------------------
    def _prepareRequest(self, req, ro):
        dac = Application.Instance().getDACache()

        if ro.format == ro.VFormatJSON:
            contentType = "application/json"
            extension = "json"
        elif ro.format == ro.VFormatGeoCSV:
            contentType = "text/csv"
            extension = "csv"
        else:
            contentType = "text/plain"
            extension = "txt"

        req.setHeader("Content-Type", contentType)
        req.setHeader(
            "Content-Disposition",
            f"inline; filename=fdsnws-ext-availability_{Time.GMT().iso()}.{extension}",
        )

        d = deferToThread(self._processRequest, req, ro, dac)

        req.notifyFinish().addErrback(utils.onCancel, d)
        d.addBoth(utils.onFinish, req)

        # The request is handled by the deferred object
        return server.NOT_DONE_YET

    # --------------------------------------------------------------------------
    @staticmethod
    def _formatTime(time, ms=False):
        if ms:
            return f"{time.toString('%FT%T')}.{time.microseconds():06d}Z"
        return time.toString("%FT%TZ")

    # --------------------------------------------------------------------------
    def _writeLines(self, req, lines, ro):
        if ro.format == ro.VFormatText:
            return self._writeFormatText(req, lines, ro)
        if ro.format == ro.VFormatGeoCSV:
            return self._writeFormatGeoCSV(req, lines, ro)
        if ro.format == ro.VFormatJSON:
            return self._writeFormatJSON(req, lines, ro)
        if ro.format == ro.VFormatRequest:
            return self._writeFormatRequest(req, lines, ro)

        raise ValueError(f"unknown reponse format: {ro.format}")

    # --------------------------------------------------------------------------
    def _writeFormatText(self, req, lines, ro):
        charCount = 0
        lineCount = 0

        # the extent service uses a different update column name and alignment
        isExtentReq = ro.__class__ == _AvailabilityExtentRequestOptions

        nslc = "{0: <2}  {1: <5}  {2: <2}  {3: <3}"
        quality = "  {0: <1}"
        sampleRate = "  {0: >11}"
        sampleRateF = "  {0: >11.1f}"
        time = "  {0: <27}  {1: <27}"
        updated = "  {0: <20}"
        timeSpans = "  {0: >10}"
        restriction = "  {0: <11}"

        header = nslc.format("#N", "S", "L", "C")
        if not ro.mergeQuality:
            header += quality.format("Q")
        if not ro.mergeSampleRate:
            header += sampleRate.format("SampleRate")
        header += time.format("Earliest", "Latest")
        if ro.showLatestUpdate:
            header += updated.format("Updated")
        if isExtentReq:
            header += timeSpans.format("TimeSpans")
            header += restriction.format("Restriction")
        header += "\n"

        first = True
        for line in lines:
            if first:
                first = False
                utils.writeTS(req, header)
                charCount += len(header)

            wid = line[0].waveformID()
            e = line[1]
            loc = wid.locationCode() if wid.locationCode() else "--"
            data = nslc.format(
                wid.networkCode(), wid.stationCode(), loc, wid.channelCode()
            )
            if not ro.mergeQuality:
                data += quality.format(e.quality())
            if not ro.mergeSampleRate:
                data += sampleRateF.format(e.sampleRate())
            data += time.format(
                self._formatTime(e.start(), True), self._formatTime(e.end(), True)
            )
            if ro.showLatestUpdate:
                data += updated.format(self._formatTime(e.updated()))
            if isExtentReq:
                data += timeSpans.format(e.segmentCount())
                data += restriction.format("RESTRICTED" if line[2] else "OPEN")
            data += "\n"

            utils.writeTS(req, data)
            charCount += len(data)
            lineCount += 1

        return charCount, lineCount

    # --------------------------------------------------------------------------
    def _writeFormatRequest(self, req, lines, ro):
        charCount = 0
        lineCount = 0

        for line in lines:
            wid = line[0].waveformID()
            e = line[1]
            loc = wid.locationCode() if wid.locationCode() else "--"
            start = e.start()
            end = e.end()

            # truncate start and end time to requested time frame
            if ro.time:
                if ro.time.start and ro.time.start > start:
                    start = ro.time.start
                if ro.time.end and ro.time.end < end:
                    end = ro.time.end

            data = (
                f"{wid.networkCode()} {wid.stationCode()} {loc} {wid.channelCode()} "
                f"{self._formatTime(start, True)} {self._formatTime(end, True)}\n"
            )

            utils.writeTS(req, data)
            charCount += len(data)
            lineCount += 1

        return charCount, lineCount

    # --------------------------------------------------------------------------
    def _writeFormatGeoCSV(self, req, lines, ro):
        charCount = 0
        lineCount = 0

        # the extent service uses a different update column name and alignment
        isExtentReq = ro.__class__ == _AvailabilityExtentRequestOptions

        nslc = "{0}|{1}|{2}|{3}"
        time = "|{0}|{1}"

        # header
        fieldUnit = "#field_unit: unitless|unitless|unitless|unitless"
        fieldType = "#field_type: string|string|string|string"
        fieldName = "Network|Station|Location|Channel"

        if not ro.mergeQuality:
            fieldUnit += "|unitless"
            fieldType += "|string"
            fieldName += "|Quality"

        if not ro.mergeSampleRate:
            fieldUnit += " |hertz"
            fieldType += " |float"
            fieldName += "|SampleRate"

        fieldUnit += " |ISO_8601|ISO_8601"
        fieldType += " |datetime|datetime"
        fieldName += "|Earliest|Latest"

        if ro.showLatestUpdate:
            fieldUnit += "|ISO_8601"
            fieldType += "|datetime"
            fieldName += "|Updated"

        if isExtentReq:
            fieldUnit += "|unitless|unitless"
            fieldType += "|integer|string"
            fieldName += "|TimeSpans|Restriction"

        header = (
            "#dataset: GeoCSV 2.0\n#delimiter: |\n"
            f"{fieldUnit}\n{fieldType}\n{fieldName}\n"
        )

        first = True
        for line in lines:
            if first:
                first = False
                utils.writeTS(req, header)
                charCount += len(header)

            wid = line[0].waveformID()
            e = line[1]
            data = nslc.format(
                wid.networkCode(),
                wid.stationCode(),
                wid.locationCode(),
                wid.channelCode(),
            )
            if not ro.mergeQuality:
                data += f"|{e.quality()}"
            if not ro.mergeSampleRate:
                data += f"|{e.sampleRate():.1f}"
            data += time.format(
                self._formatTime(e.start(), True), self._formatTime(e.end(), True)
            )
            if ro.showLatestUpdate:
                data += f"|{self._formatTime(e.updated())}"
            if isExtentReq:
                data += f"|{e.segmentCount():d}"
                data += "|RESTRICTED" if line[2] else "|OPEN"
            data += "\n"

            utils.writeTS(req, data)
            charCount += len(data)
            lineCount += 1

        return charCount, lineCount

    # --------------------------------------------------------------------------
    def _writeFormatJSON(self, req, lines, ro):
        header = (
            f'{{"created":"{self._formatTime(Time.GMT())}",'
            '"version": 1.0,'
            '"datasources":['
        )
        footer = "]}"

        return self._writeJSONChannels(req, header, footer, lines, ro)


###############################################################################
@implementer(portal.IRealm)
class FDSNAvailabilityExtentRealm:
    # --------------------------------------------------------------------------
    def __init__(self, access):
        self.__access = access

    # --------------------------------------------------------------------------
    def requestAvatar(self, avatarId, _mind, *interfaces):
        if resource.IResource in interfaces:
            user = {"mail": utils.u_str(avatarId), "blacklisted": False}
            return (
                resource.IResource,
                FDSNAvailabilityExtent(self.__access, user),
                lambda: None,
            )

        raise NotImplementedError()


###############################################################################
@implementer(portal.IRealm)
class FDSNAvailabilityExtentAuthRealm:
    # --------------------------------------------------------------------------
    def __init__(self, access, userdb):
        self.__access = access
        self.__userdb = userdb

    # --------------------------------------------------------------------------
    def requestAvatar(self, avatarId, _mind, *interfaces):
        if resource.IResource in interfaces:
            user = self.__userdb.getAttributes(utils.u_str(avatarId))
            return (
                resource.IResource,
                FDSNAvailabilityExtent(self.__access, user),
                lambda: None,
            )

        raise NotImplementedError()


###############################################################################
class FDSNAvailabilityExtent(_Availability):
    isLeaf = True

    # --------------------------------------------------------------------------
    @staticmethod
    def _createRequestOptions():
        return _AvailabilityExtentRequestOptions()

    # --------------------------------------------------------------------------
    @staticmethod
    def _mergeExtents(attributeExtents):
        merged = None
        cloned = False

        # Create a copy of the extent only if more than 1 element is found. The number
        # of elements is not known in advance since attributeExtents might only be an
        # iterator.
        for attExt in attributeExtents:
            if merged is None:
                merged = attExt
                continue

            if not cloned:
                merged = datamodel.DataAttributeExtent(merged)
                cloned = True

            if attExt.start() < merged.start():
                merged.setStart(attExt.start())
            if attExt.end() > merged.end():
                merged.setEnd(attExt.end())
            if attExt.updated() > merged.updated():
                merged.setUpdated(attExt.updated())
            merged.setSegmentCount(merged.segmentCount() + attExt.segmentCount())

        return merged

    # --------------------------------------------------------------------------
    def _writeJSONChannels(self, req, header, footer, lines, ro):
        charCount = 0

        nslc = (
            "{{"
            '"network":"{0}",'
            '"station":"{1}",'
            '"location":"{2}",'
            '"channel":"{3}"'
        )
        quality = ',"quality":"{0}"'
        sampleRate = ',"samplerate":{0}'
        time = (
            ',"earliest":"{0}","latest":"{1}","timespanCount":{2}'
            ',"updated":"{3}","restriction":"{4}"}}'
        )

        utils.writeTS(req, header)
        charCount += len(header)

        data = None
        for line in lines:
            wid = line[0].waveformID()
            e = line[1]
            data = "," if data is not None else ""
            data += nslc.format(
                wid.networkCode(),
                wid.stationCode(),
                wid.locationCode(),
                wid.channelCode(),
            )
            if not ro.mergeQuality:
                data += quality.format(e.quality())
            if not ro.mergeSampleRate:
                data += sampleRate.format(e.sampleRate())
            data += time.format(
                self._formatTime(e.start(), True),
                self._formatTime(e.end(), True),
                e.segmentCount(),
                self._formatTime(e.updated()),
                format("RESTRICTED" if line[2] else "OPEN"),
            )

            utils.writeTS(req, data)
            charCount += len(data)

        utils.writeTS(req, footer)
        charCount += len(footer)

        return charCount, len(lines)

    # --------------------------------------------------------------------------
    def _processRequest(self, req, ro, dac):
        if req._disconnected:  # pylint: disable=W0212
            return False

        # tuples: extent, attribute extent, restricted status
        lines = []

        mergeAll = ro.mergeQuality and ro.mergeSampleRate
        mergeNone = not ro.mergeQuality and not ro.mergeSampleRate

        # iterate extents
        for ext, _, restricted in ro.extentIter(dac, self.user, self.access):
            if req._disconnected:  # pylint: disable=W0212
                return False

            # iterate attribute extents and merge them if requested
            if mergeNone:
                for attExt in ro.attributeExtentIter(ext):
                    lines.append((ext, attExt, restricted))
                continue

            if mergeAll:
                attExt = self._mergeExtents(ro.attributeExtentIter(ext))
                if attExt is not None:
                    lines.append((ext, attExt, restricted))
                continue

            attExtDict = {}
            if ro.mergeQuality:
                # key=sampleRate
                for attExt in ro.attributeExtentIter(ext):
                    if attExt.sampleRate() in attExtDict:
                        attExtDict[attExt.sampleRate()].append(attExt)
                    else:
                        attExtDict[attExt.sampleRate()] = [attExt]
            # ro.mergeSampleRate
            else:
                # key=quality
                for attExt in ro.attributeExtentIter(ext):
                    if attExt.quality() in attExtDict:
                        attExtDict[attExt.quality()].append(attExt)
                    else:
                        attExtDict[attExt.quality()] = [attExt]

            for attExtents in attExtDict.values():
                lines.append((ext, self._mergeExtents(attExtents), restricted))

        # Return 204 if no matching availability information was found
        if not lines:
            msg = "no matching availabilty information found"
            self.writeErrorPage(req, http.NO_CONTENT, msg, ro)
            return True

        # sort lines
        self._sortLines(lines, ro)

        # truncate lines to requested row limit
        if ro.limit:
            del lines[ro.limit :]

        charCount, extCount = self._writeLines(req, lines, ro)

        logging.debug(
            f"{ro.service}: returned {extCount} extents (characters: {charCount})"
        )
        utils.accessLog(req, ro, http.OK, charCount, None)
        return True

    # --------------------------------------------------------------------------
    @staticmethod
    def _sortLines(lines, ro):
        def compareNSLC(l1, l2):
            if l1[0] is not l2[0]:
                # The lines are expected to be sorted according NSLC
                return 0

            e1 = l1[1]
            e2 = l2[1]

            if e1.start() < e2.start():
                return -1
            if e1.start() > e2.start():
                return 1
            if e1.end() < e2.end():
                return -1
            if e1.end() > e2.end():
                return 1

            if not ro.mergeQuality:
                if e1.quality() < e2.quality():
                    return -1
                if e1.quality() > e2.quality():
                    return 1

            if not ro.mergeSampleRate:
                if e1.sampleRate() < e2.sampleRate():
                    return -1
                if e1.sampleRate() > e2.sampleRate():
                    return 1

            return 0

        def compareCount(l1, l2):
            c1 = l1[1].segmentCount()
            c2 = l2[1].segmentCount()

            return -1 if c1 < c2 else 1 if c1 > c2 else compareNSLC(l1, l2)

        def compareCountDesc(l1, l2):
            c1 = l1[1].segmentCount()
            c2 = l2[1].segmentCount()

            return -1 if c1 > c2 else 1 if c1 < c2 else compareNSLC(l1, l2)

        def compareUpdate(l1, l2):
            c1 = l1[1].updated()
            c2 = l2[1].updated()

            return -1 if c1 < c2 else 1 if c1 > c2 else compareNSLC(l1, l2)

        def compareUpdateDesc(l1, l2):
            c1 = l1[1].updated()
            c2 = l2[1].updated()

            return -1 if c1 > c2 else 1 if c1 < c2 else compareNSLC(l1, l2)

        comparator = (
            compareNSLC
            if ro.orderBy == ro.VOrderByNSLC
            else compareCount
            if ro.orderBy == ro.VOrderByCount
            else compareCountDesc
            if ro.orderBy == ro.VOrderByCountDesc
            else compareUpdate
            if ro.orderBy == ro.VOrderByUpdate
            else compareUpdateDesc
        )
        lines.sort(key=cmp_to_key(comparator))


###############################################################################
@implementer(portal.IRealm)
class FDSNAvailabilityQueryRealm:
    # --------------------------------------------------------------------------
    def __init__(self, access):
        self.__access = access

    # --------------------------------------------------------------------------
    def requestAvatar(self, avatarId, _mind, *interfaces):
        if resource.IResource in interfaces:
            user = {"mail": utils.u_str(avatarId), "blacklisted": False}
            return (
                resource.IResource,
                FDSNAvailabilityQuery(self.__access, user),
                lambda: None,
            )

        raise NotImplementedError()


###############################################################################
@implementer(portal.IRealm)
class FDSNAvailabilityQueryAuthRealm:
    # --------------------------------------------------------------------------
    def __init__(self, access, userdb):
        self.__access = access
        self.__userdb = userdb

    # --------------------------------------------------------------------------
    def requestAvatar(self, avatarId, _mind, *interfaces):
        if resource.IResource in interfaces:
            user = self.__userdb.getAttributes(utils.u_str(avatarId))
            return (
                resource.IResource,
                FDSNAvailabilityQuery(self.__access, user),
                lambda: None,
            )

        raise NotImplementedError()


###############################################################################
class FDSNAvailabilityQuery(_Availability):
    isLeaf = True

    # --------------------------------------------------------------------------
    @staticmethod
    def _createRequestOptions():
        return _AvailabilityQueryRequestOptions()

    # --------------------------------------------------------------------------
    def _writeJSONChannels(self, req, header, footer, lines, ro):
        charCount = 0
        lineCount = 0

        nslc = (
            "{{"
            '"network":"{0}",'
            '"station":"{1}",'
            '"location":"{2}",'
            '"channel":"{3}"'
        )
        quality = ',"quality":"{0}"'
        sampleRate = ',"samplerate":{0}'
        updated = ',"updated":"{0}"'
        timeSpans = ',"timespans":[['
        seg = '"{0}","{1}"]'

        class SegGroup:
            def __init__(self, segments, updated):
                self.segments = segments
                self.updated = updated

        def writeSegments(segments):
            first = True
            charCount = 0

            for s in segments:
                if first:
                    first = False
                    data = timeSpans
                else:
                    data = ",["

                data += seg.format(
                    self._formatTime(s.start(), True), self._formatTime(s.end(), True)
                )
                utils.writeTS(req, data)
                charCount += len(data)

            return charCount

        prevExt = None

        # merge of quality and sample rate: all timespans of one stream are
        # grouped into one channel
        if ro.mergeQuality and ro.mergeSampleRate:
            lastUpdate = None
            segments = None
            for ext, attExt, _restricted in lines:
                lineCount += 1

                if ext is prevExt:
                    if attExt.updated() > lastUpdate:
                        lastUpdate = attExt.updated()
                    segments.append(attExt)
                    continue

                if prevExt is not None:
                    if charCount == 0:
                        utils.writeTS(req, header)
                        charCount += len(header)
                        data = ""
                    else:
                        data = "]},"

                    wid = prevExt.waveformID()
                    data += nslc.format(
                        wid.networkCode(),
                        wid.stationCode(),
                        wid.locationCode(),
                        wid.channelCode(),
                    )
                    if ro.showLatestUpdate:
                        data += updated.format(self._formatTime(lastUpdate))
                    utils.writeTS(req, data)
                    charCount += len(data)
                    charCount += writeSegments(segments)

                prevExt = ext
                lastUpdate = attExt.updated()
                segments = [attExt]

            # handle last extent
            if prevExt is not None:
                if charCount == 0:
                    utils.writeTS(req, header)
                    charCount += len(header)
                    data = ""
                else:
                    data = "]},"

                wid = prevExt.waveformID()
                data += nslc.format(
                    wid.networkCode(),
                    wid.stationCode(),
                    wid.locationCode(),
                    wid.channelCode(),
                )
                if ro.showLatestUpdate:
                    data += updated.format(self._formatTime(lastUpdate))
                utils.writeTS(req, data)
                charCount += len(data)
                charCount += writeSegments(segments)

                data = "]}"
                utils.writeTS(req, data)
                charCount += len(data)
                utils.writeTS(req, footer)
                charCount += len(footer)

        # merge of quality: all timespans of one stream are grouped by sample rate
        elif ro.mergeQuality:
            segGroups = OrderedDict()
            for ext, attExt, _restricted in lines:
                lineCount += 1

                if ext is prevExt:
                    if attExt.sampleRate() in segGroups:
                        segGroup = segGroups[attExt.sampleRate()]
                        if attExt.updated() > segGroup.updated:
                            segGroup.updated = attExt.updated()
                        segGroup.segments.append(attExt)
                        continue

                else:
                    if prevExt is not None:
                        wid = prevExt.waveformID()
                        for sr, sg in segGroups.items():
                            if req._disconnected:  # pylint: disable=W0212
                                return False

                            if charCount == 0:
                                utils.writeTS(req, header)
                                charCount += len(header)
                                data = ""
                            else:
                                data = "]},"

                            data += nslc.format(
                                wid.networkCode(),
                                wid.stationCode(),
                                wid.locationCode(),
                                wid.channelCode(),
                            )
                            data += sampleRate.format(sr)
                            if ro.showLatestUpdate:
                                data += updated.format(self._formatTime(sg.updated))
                            utils.writeTS(req, data)
                            charCount += len(data)
                            charCount += writeSegments(sg.segments)

                    prevExt = ext
                    segGroups = OrderedDict()

                segGroups[attExt.sampleRate()] = SegGroup([attExt], attExt.updated())

            # handle last extent
            if prevExt is not None:
                wid = prevExt.waveformID()
                for sr, sg in segGroups.items():
                    if req._disconnected:  # pylint: disable=W0212
                        return False

                    if charCount == 0:
                        utils.writeTS(req, header)
                        charCount += len(header)
                        data = ""
                    else:
                        data = "]},"

                    data += nslc.format(
                        wid.networkCode(),
                        wid.stationCode(),
                        wid.locationCode(),
                        wid.channelCode(),
                    )
                    data += sampleRate.format(sr)
                    if ro.showLatestUpdate:
                        data += updated.format(self._formatTime(sg.updated))
                    utils.writeTS(req, data)
                    charCount += len(data)
                    charCount += writeSegments(sg.segments)

                data = "]}"
                utils.writeTS(req, data)
                charCount += len(data)
                utils.writeTS(req, footer)
                charCount += len(footer)

        # merge of sample rate: all timespans of one stream are grouped by
        # quality
        elif ro.mergeSampleRate:
            segGroups = OrderedDict()
            for ext, attExt, _restricted in lines:
                lineCount += 1

                if ext is prevExt:
                    if attExt.quality() in segGroups:
                        segGroup = segGroups[attExt.quality()]
                        if attExt.updated() > segGroup.updated:
                            segGroup.updated = attExt.updated()
                        segGroup.segments.append(attExt)
                        continue

                else:
                    if prevExt is not None:
                        wid = prevExt.waveformID()
                        for q, sg in segGroups.items():
                            if req._disconnected:  # pylint: disable=W0212
                                return False

                            if charCount == 0:
                                utils.writeTS(req, header)
                                charCount += len(header)
                                data = ""
                            else:
                                data = "]},"

                            data += nslc.format(
                                wid.networkCode(),
                                wid.stationCode(),
                                wid.locationCode(),
                                wid.channelCode(),
                            )
                            data += quality.format(q)
                            if ro.showLatestUpdate:
                                data += updated.format(self._formatTime(sg.updated))
                            utils.writeTS(req, data)
                            charCount += len(data)
                            charCount += writeSegments(sg.segments)

                    prevExt = ext
                    segGroups = OrderedDict()

                segGroups[attExt.quality()] = SegGroup([attExt], attExt.updated())

            # handle last extent
            if prevExt is not None:
                wid = prevExt.waveformID()
                for q, sg in segGroups.items():
                    if req._disconnected:  # pylint: disable=W0212
                        return False

                    if charCount == 0:
                        utils.writeTS(req, header)
                        charCount += len(header)
                        data = ""
                    else:
                        data = "]},"

                    data += nslc.format(
                        wid.networkCode(),
                        wid.stationCode(),
                        wid.locationCode(),
                        wid.channelCode(),
                    )
                    data += quality.format(q)
                    if ro.showLatestUpdate:
                        data += updated.format(self._formatTime(sg.updated))
                    utils.writeTS(req, data)
                    charCount += len(data)
                    charCount += writeSegments(sg.segments)

                data = "]}"
                utils.writeTS(req, data)
                charCount += len(data)
                utils.writeTS(req, footer)
                charCount += len(footer)

        # no merge: all timespans of one stream are grouped by tuple of quality and
        # sampleRate
        else:
            segGroups = OrderedDict()
            for ext, attExt, _restricted in lines:
                lineCount += 1
                t = (attExt.quality(), attExt.sampleRate())

                if ext is prevExt:
                    if t in segGroups:
                        segGroup = segGroups[t]
                        if attExt.updated() > segGroup.updated:
                            segGroup.updated = attExt.updated()
                        segGroup.segments.append(attExt)
                        continue

                else:
                    if prevExt is not None:
                        wid = prevExt.waveformID()
                        for (q, sr), sg in segGroups.items():
                            if req._disconnected:  # pylint: disable=W0212
                                return False

                            if charCount == 0:
                                utils.writeTS(req, header)
                                charCount += len(header)
                                data = ""
                            else:
                                data = "]},"

                            data += nslc.format(
                                wid.networkCode(),
                                wid.stationCode(),
                                wid.locationCode(),
                                wid.channelCode(),
                            )
                            data += quality.format(q)
                            data += sampleRate.format(sr)
                            if ro.showLatestUpdate:
                                data += updated.format(self._formatTime(sg.updated))
                            utils.writeTS(req, data)
                            charCount += len(data)
                            charCount += writeSegments(sg.segments)

                    prevExt = ext
                    segGroups = OrderedDict()

                segGroups[t] = SegGroup([attExt], attExt.updated())

            # handle last extent
            if prevExt is not None:
                wid = prevExt.waveformID()
                for (q, sr), sg in segGroups.items():
                    if req._disconnected:  # pylint: disable=W0212
                        return False

                    if charCount == 0:
                        utils.writeTS(req, header)
                        charCount += len(header)
                        data = ""
                    else:
                        data = "]},"

                    data += nslc.format(
                        wid.networkCode(),
                        wid.stationCode(),
                        wid.locationCode(),
                        wid.channelCode(),
                    )
                    data += quality.format(q)
                    data += sampleRate.format(sr)
                    if ro.showLatestUpdate:
                        data += updated.format(self._formatTime(sg.updated))
                    utils.writeTS(req, data)
                    charCount += len(data)
                    charCount += writeSegments(sg.segments)

                data = "]}"
                utils.writeTS(req, data)
                charCount += len(data)
                utils.writeTS(req, footer)
                charCount += len(footer)

        return charCount, lineCount

    # --------------------------------------------------------------------------
    def _processRequest(self, req, ro, dac):
        if req._disconnected:  # pylint: disable=W0212
            return False

        # tuples: wid, segment, restricted status
        lines = []

        charCount = 0

        # iterate extents and create IN clauses of parent_oids in bunches
        # of 1000 because the query size is limited
        parentOIDs, idList, tooLarge = [], [], []
        i = 0
        for ext, objID, _ in ro.extentIter(dac, self.user, self.access):
            if req._disconnected:  # pylint: disable=W0212
                return False

            if ro.excludeTooLarge:
                if ext.segmentOverflow():
                    continue
            elif ext.segmentOverflow():
                tooLarge.append(ext)
                continue
            elif tooLarge:
                continue

            if i < 1000:
                idList.append(objID)
                i += 1
            else:
                parentOIDs.append(idList)
                idList = [objID]
                i = 1

        if not ro.excludeTooLarge and tooLarge:
            extents = ", ".join(
                f"{e.waveformID().networkCode()}.{e.waveformID().stationCode()}."
                f"{e.waveformID().locationCode()}.{e.waveformID().channelCode()}"
                for e in tooLarge
            )

            msg = (
                "Unable to process request due to database limitations. Some "
                "selections have too many segments to process. Rejected extents: "
                f"{{{extents}}}. This limitation may be resolved in a future version "
                "of this webservice."
            )
            self.writeErrorPage(req, http.REQUEST_ENTITY_TOO_LARGE, msg, ro)
            return False

        if len(idList) > 0:
            parentOIDs.append(idList)
        else:
            msg = "no matching availabilty information found"
            self.writeErrorPage(req, http.NO_CONTENT, msg, ro)
            return False

        db = io.DatabaseInterface.Open(Application.Instance().databaseURI())
        if db is None:
            msg = "could not connect to database"
            return self.renderErrorPage(req, http.SERVICE_UNAVAILABLE, msg, ro)

        lines = self._lineIter(db, parentOIDs, req, ro, dac.extentsOID())

        charCount, segCount = self._writeLines(req, lines, ro)

        # Return 204 if no matching availability information was found
        if segCount <= 0:
            msg = "no matching availabilty information found"
            self.writeErrorPage(req, http.NO_CONTENT, msg, ro)
            return True

        logging.debug(
            f"{ro.service}: returned {segCount} segments (characters: {charCount})"
        )
        utils.accessLog(req, ro, http.OK, charCount, None)

        return True

    # --------------------------------------------------------------------------
    @staticmethod
    def _lineIter(db, parentOIDs, req, ro, oIDs):
        def _T(name):
            return db.convertColumnName(name)

        dba = datamodel.DatabaseArchive(db)

        for idList in parentOIDs:
            if req._disconnected:  # pylint: disable=W0212
                return

            # build SQL query
            idStr = ",".join(str(x) for x in idList)
            q = f"SELECT * from DataSegment WHERE _parent_oid IN ({idStr}) "
            if ro.time:
                if ro.time.start is not None:
                    if ro.time.start.microseconds() == 0:
                        q += f"AND {_T('end')} >= '{db.timeToString(ro.time.start)}' "
                    else:
                        q += (
                            "AND ({0} > '{1}' OR ("
                            f"{_T('end')} = '{db.timeToString(ro.time.start)}' AND "
                            f"end_ms >= {ro.time.start.microseconds()})) "
                        )
                if ro.time.end is not None:
                    if ro.time.end.microseconds() == 0:
                        q += f"AND {_T('start')} < '{db.timeToString(ro.time.end)}' "
                    else:
                        q += (
                            "AND ({0} < '{1}' OR ("
                            f"{_T('start')} = '{db.timeToString(ro.time.end)}' AND "
                            "start_ms < {ro.time.end.microseconds()})) "
                        )
            if ro.quality:
                qualities = "', '".join(ro.quality)
                q += f"AND {_T('quality')} IN ('{qualities}') "
            q += f"ORDER BY _parent_oid, {_T('start')}, {_T('start_ms')}"

            segIt = dba.getObjectIterator(q, datamodel.DataSegment.TypeInfo())
            if segIt is None or not segIt.valid():
                return

            # Iterate and optionally merge segments.
            # A segment will be yielded if
            # - the extent changed
            # - quality changed and merging of quality was not requested
            # - sample rate changed and merging of sample rate was not
            #   requested
            # - an overlap was detected and merging of overlaps was not
            #   requested
            # - an gap was detected and the gaps exceeds the mergeGaps
            #   threshold
            seg = None
            ext = None
            lines = 0
            while (
                not req._disconnected  # pylint: disable=W0212
                and (seg is None or segIt.next())
                and (not ro.limit or lines < ro.limit)
            ):
                s = datamodel.DataSegment.Cast(segIt.get())
                if s is None:
                    break

                # get extent for current segment
                try:
                    e, restricted = oIDs[segIt.parentOid()]
                except KeyError:
                    logging.warning(f"parent object id not found: {segIt.parentOid()}")
                    continue

                # first segment, no merge test required
                if seg is None:
                    seg = s
                    ext = e
                    jitter = 1 / (2 * s.sampleRate())
                    continue

                # merge test
                diff = float(s.start() - seg.end())
                mQuality = ro.mergeQuality or s.quality() == seg.quality()
                mSampleRate = ro.mergeSampleRate or s.sampleRate() == seg.sampleRate()
                mGaps = (ro.mergeGaps is None and diff <= jitter) or (
                    ro.mergeGaps is not None and diff <= ro.mergeGaps
                )
                mOverlap = -diff <= jitter or ro.mergeOverlap
                if e is ext and mQuality and mSampleRate and mGaps and mOverlap:
                    seg.setEnd(s.end())
                    if s.updated() > seg.updated():
                        seg.setUpdated(s.updated())
                    continue

                # merge was not possible, yield previous segment
                yield (ext, seg, restricted)
                lines += 1
                seg = s
                ext = e
                if seg.sampleRate() != s.sampleRate():
                    jitter = 1 / (2 * s.sampleRate())

            if seg is not None and (not ro.limit or lines < ro.limit):
                yield (ext, seg, restricted)

            # close database iterator if iteration was stopped because of
            # row limit
            return


# vim: ts=4 et tw=79
