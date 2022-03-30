################################################################################
# Copyright (C) 2013-2014 gempa GmbH
#
# Common utility functions
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
################################################################################

from __future__ import absolute_import, division, print_function

import socket
import sys
import traceback

from twisted.internet import reactor, defer
from twisted.python.failure import Failure

import seiscomp.logging
import seiscomp.core
import seiscomp.io
from seiscomp.client import Application

#-------------------------------------------------------------------------------
# Converts a unicode string to a byte string
b_str = lambda s: s.encode('utf-8', 'replace')


#-------------------------------------------------------------------------------
# Converts a byte string to a unicode string
u_str = lambda s: s.decode('utf-8', 'replace')


#-------------------------------------------------------------------------------
# Python version depended string conversion
if sys.version_info[0] < 3:
    py2bstr = b_str
    py2ustr = u_str
    py3bstr = str
    py3ustr = str
    py3ustrlist = lambda l: l
else:
    py2bstr = str
    py2ustr = str
    py3bstr = b_str
    py3ustr = u_str
    py3ustrlist = lambda l: [u_str(x) for x in l]


#-------------------------------------------------------------------------------
# Tests if a SC3 inventory object is restricted
def isRestricted(obj):
    try:
        return obj.restricted()
    except ValueError:
        return False


#-------------------------------------------------------------------------------
# Thread-safe write of string data using reactor main thread
def writeTS(req, data):
    reactor.callFromThread(req.write, py3bstr(data))


#-------------------------------------------------------------------------------
# Thread-safe write of binary data using reactor main thread
def writeTSBin(req, data):
    reactor.callFromThread(req.write, data)


#-------------------------------------------------------------------------------
# Finish requests deferred to threads
def onFinish(result, req):
    seiscomp.logging.debug("finish value = %s" % str(result))
    if isinstance(result, Failure):
        err = result.value
        if isinstance(err, defer.CancelledError):
            seiscomp.logging.error("request canceled")
            return
        seiscomp.logging.error("%s %s" % (
            result.getErrorMessage(), traceback.format_tb(result.getTracebackObject())))
    else:
        if result:
            seiscomp.logging.debug("request successfully served")
        else:
            seiscomp.logging.debug("request failed")

    reactor.callFromThread(req.finish)


#-------------------------------------------------------------------------------
# Handle connection errors
def onCancel(failure, req):
    if failure:
        seiscomp.logging.error("%s %s" % (
            failure.getErrorMessage(), traceback.format_tb(failure.getTracebackObject())))
    else:
        seiscomp.logging.error("request canceled")
    req.cancel()


#-------------------------------------------------------------------------------
# Handle premature connection reset
def onResponseFailure(_, call):
    seiscomp.logging.error("response canceled")
    call.cancel()


#-------------------------------------------------------------------------------
# Renders error page if the result set exceeds the configured maximum number
# objects
def accessLog(req, ro, code, length, err):
    logger = Application.Instance()._accessLog # pylint: disable=W0212
    if logger is None:
        return

    logger.log(AccessLogEntry(req, ro, code, length, err))


################################################################################
class Sink(seiscomp.io.ExportSink):
    def __init__(self, request):
        seiscomp.io.ExportSink.__init__(self)
        self.request = request
        self.written = 0

    def write(self, data):
        if self.request._disconnected: #pylint: disable=W0212
            return -1
        writeTSBin(self.request, data)
        self.written += len(data)
        return len(data)


################################################################################
class AccessLogEntry:
    def __init__(self, req, ro, code, length, err):
        # user agent
        agent = req.getHeader("User-Agent")
        if agent is None:
            agent = ""
        else:
            agent = agent[:100].replace('|', ' ')

        if err is None:
            err = ""

        service, user, accessTime, procTime = "", "", "", 0
        net, sta, loc, cha = "", "", "", ""
        if ro is not None:
            # processing time in milliseconds
            procTime = int((seiscomp.core.Time.GMT() - ro.accessTime).length() * 1000.0)

            service = ro.service
            if ro.userName is not None:
                user = ro.userName
            accessTime = str(ro.accessTime)

            if ro.channel is not None:
                if ro.channel.net is not None:
                    net = ",".join(ro.channel.net)
                if ro.channel.sta is not None:
                    sta = ",".join(ro.channel.sta)
                if ro.channel.loc is not None:
                    loc = ",".join(ro.channel.loc)
                if ro.channel.cha is not None:
                    cha = ",".join(ro.channel.cha)

        # The host name of the client is resolved in the __str__ method by the
        # logging thread so that a long running DNS reverse lookup may not slow
        # down the request
        self.msgPrefix = "%s|%s|%s|" % (
            service, py3ustr(req.getRequestHostname()), accessTime)

        xff = req.requestHeaders.getRawHeaders("x-forwarded-for")
        if xff:
            self.userIP = xff[0].split(",")[0].strip()
        else:
            self.userIP = req.getClientIP()

        self.clientIP = req.getClientIP()
        self.msgSuffix = "|%s|%i|%i|%s|%s|%i|%s|%s|%s|%s|%s||" % (
            self.clientIP, length, procTime, err, agent, code, user, net, sta, loc, cha)

    def __str__(self):
        try:
            userHost = socket.gethostbyaddr(self.userIP)[0]
        except socket.herror:
            userHost = self.userIP
        return self.msgPrefix + userHost + self.msgSuffix


# vim: ts=4 et
