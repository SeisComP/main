################################################################################
# Copyright (C) 2013-2014 gempa GmbH
#
# Common utility functions
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
################################################################################

import socket
import traceback

from twisted.internet import reactor, defer
from twisted.python.failure import Failure

import seiscomp.logging
import seiscomp.core
import seiscomp.io
from seiscomp.client import Application


# -------------------------------------------------------------------------------
# Converts a unicode string to a byte string
def b_str(unicode_string):
    return unicode_string.encode("utf-8", "replace")


# -------------------------------------------------------------------------------
# Converts a byte string to a unicode string
def u_str(byte_string):
    return byte_string.decode("utf-8", "replace")


# -------------------------------------------------------------------------------
# Tests if a SC3 inventory object is restricted
def isRestricted(obj):
    try:
        return obj.restricted()
    except ValueError:
        return False


# -------------------------------------------------------------------------------
# Thread-safe write of string data using reactor main thread
def writeTS(req, data):
    reactor.callFromThread(req.write, b_str(data))


# -------------------------------------------------------------------------------
# Thread-safe write of binary data using reactor main thread
def writeTSBin(req, data):
    reactor.callFromThread(req.write, data)


# -------------------------------------------------------------------------------
# Finish requests deferred to threads
def onFinish(result, req):
    seiscomp.logging.debug(f"finish value = {str(result)}")
    if isinstance(result, Failure):
        err = result.value
        if isinstance(err, defer.CancelledError):
            seiscomp.logging.error("request canceled")
            return
        seiscomp.logging.error(
            f"{result.getErrorMessage()} "
            f"{traceback.format_tb(result.getTracebackObject())}"
        )
    else:
        if result:
            seiscomp.logging.debug("request successfully served")
        else:
            seiscomp.logging.debug("request failed")

    reactor.callFromThread(req.finish)


# -------------------------------------------------------------------------------
# Handle connection errors
def onCancel(failure, req):
    if failure:
        seiscomp.logging.error(
            f"{failure.getErrorMessage()} "
            f"{traceback.format_tb(failure.getTracebackObject())}"
        )
    else:
        seiscomp.logging.error("request canceled")
    req.cancel()


# -------------------------------------------------------------------------------
# Handle premature connection reset
def onResponseFailure(_, call):
    seiscomp.logging.error("response canceled")
    call.cancel()


# -------------------------------------------------------------------------------
# Renders error page if the result set exceeds the configured maximum number
# objects
def accessLog(req, ro, code, length, err):
    logger = Application.Instance()._accessLog  # pylint: disable=W0212
    if logger is None:
        return

    logger.log(AccessLogEntry(req, ro, code, length, err))


################################################################################
class Sink(seiscomp.io.ExportSink):
    def __init__(self, request):
        super().__init__()

        self.request = request
        self.written = 0

    def write(self, data):
        if self.request._disconnected:  # pylint: disable=W0212
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
            agent = agent[:100].replace("|", " ")

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
        self.msgPrefix = f"{service}|{u_str(req.getRequestHostname())}|{accessTime}|"

        xff = req.requestHeaders.getRawHeaders("x-forwarded-for")
        if xff:
            self.userIP = xff[0].split(",")[0].strip()
        else:
            self.userIP = req.getClientIP()

        self.clientIP = req.getClientIP()
        self.msgSuffix = (
            f"|{self.clientIP}|{length}|{procTime}|{err}|{agent}|{code}|{user}|{net}"
            f"|{sta}|{loc}|{cha}||"
        )

    def __str__(self):
        try:
            userHost = socket.gethostbyaddr(self.userIP)[0]
        except socket.herror:
            userHost = self.userIP
        return self.msgPrefix + userHost + self.msgSuffix


# vim: ts=4 et
