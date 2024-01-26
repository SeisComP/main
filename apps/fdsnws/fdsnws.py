#!/usr/bin/env seiscomp-python

################################################################################
# Copyright (C) 2013-2014 gempa GmbH
#
# FDSNWS -- Implements FDSN Web Service interface, see
# http://www.fdsn.org/webservices/
#
# Implemented Services:
#   fdsnws-dataselect
#   fdsnws-event
#   fdsnws-station
#   fdsnws-availability
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import base64
import fnmatch
import os
import re
import signal
import sys
import time

try:
    from twisted.cred import checkers, credentials, error, portal
    from twisted.internet import reactor, defer, task
    from twisted.web import guard, static
    from twisted.python import log, failure
    from zope.interface import implementer
except ImportError as importErr:
    sys.exit(f"{importErr}\nIs python twisted installed?")

import seiscomp.core
import seiscomp.datamodel
import seiscomp.io
import seiscomp.logging
import seiscomp.client
import seiscomp.system

from seiscomp.math import KM_OF_DEGREE

from seiscomp.fdsnws.utils import isRestricted, u_str, b_str
from seiscomp.fdsnws.dataselect import (
    FDSNDataSelect,
    FDSNDataSelectRealm,
    FDSNDataSelectAuthRealm,
)
from seiscomp.fdsnws.dataselect import VERSION as DataSelectVersion
from seiscomp.fdsnws.event import FDSNEvent
from seiscomp.fdsnws.event import VERSION as EventVersion
from seiscomp.fdsnws.station import FDSNStation
from seiscomp.fdsnws.station import VERSION as StationVersion
from seiscomp.fdsnws.availability import (
    FDSNAvailabilityQuery,
    FDSNAvailabilityQueryRealm,
    FDSNAvailabilityQueryAuthRealm,
    FDSNAvailabilityExtent,
    FDSNAvailabilityExtentRealm,
    FDSNAvailabilityExtentAuthRealm,
)
from seiscomp.fdsnws.availability import VERSION as AvailabilityVersion
from seiscomp.fdsnws.http import (
    DirectoryResource,
    ListingResource,
    NoResource,
    Site,
    ServiceVersion,
    AuthResource,
    WADLFilter,
)
from seiscomp.fdsnws.log import Log


def logSC3(entry):
    try:
        isError = entry["isError"]
        msg = entry["message"]
        if isError:
            for line in msg:
                seiscomp.logging.error(f"[reactor] {line}")
        else:
            for line in msg:
                seiscomp.logging.info(f"[reactor] {line}")
    except Exception:
        pass


###############################################################################
# Make CORS work with queryauth
class HTTPAuthSessionWrapper(guard.HTTPAuthSessionWrapper):
    def render(self, request):
        if request.method == b"OPTIONS":
            request.setHeader(b"Allow", b"GET,HEAD,POST,OPTIONS")
            return b""

        return guard.HTTPAuthSessionWrapper.render(self, request)


###############################################################################
@implementer(checkers.ICredentialsChecker)
class UsernamePasswordChecker:
    credentialInterfaces = (
        credentials.IUsernamePassword,
        credentials.IUsernameHashedPassword,
    )

    # -------------------------------------------------------------------------
    def __init__(self, userdb):
        self.__userdb = userdb

    # -------------------------------------------------------------------------
    @staticmethod
    def __cbPasswordMatch(matched, username):
        if matched:
            return username

        return failure.Failure(error.UnauthorizedLogin())

    # -------------------------------------------------------------------------
    def requestAvatarId(self, cred):
        return defer.maybeDeferred(self.__userdb.checkPassword, cred).addCallback(
            self.__cbPasswordMatch, cred.username
        )


###############################################################################
class UserDB:
    # -------------------------------------------------------------------------
    def __init__(self):
        self.__users = {}
        self.__blacklist = set()
        task.LoopingCall(self.__expireUsers).start(60, False)

    # -------------------------------------------------------------------------
    def __expireUsers(self):
        for name, (_, _, expires) in list(self.__users.items()):
            if time.time() > expires:
                seiscomp.logging.info(f"de-registering {name}")
                del self.__users[name]

    # -------------------------------------------------------------------------
    def blacklistUser(self, name):
        seiscomp.logging.info(f"blacklisting {name}")
        self.__blacklist.add(name)

    # -------------------------------------------------------------------------
    def addUser(self, name, attributes, expires, data):
        try:
            password = self.__users[name][0]

        except KeyError:
            bl = " (blacklisted)" if name in self.__blacklist else ""
            seiscomp.logging.notice(f"registering {name}{bl} {data}")
            password = base64.urlsafe_b64encode(os.urandom(12))

        attributes["blacklisted"] = name in self.__blacklist
        self.__users[name] = (password, attributes, expires)
        return password

    # -------------------------------------------------------------------------
    def checkPassword(self, cred):
        try:
            pw = self.__users[cred.username][0]

        except KeyError:
            return False

        return cred.checkPassword(pw)

    # -------------------------------------------------------------------------
    def getAttributes(self, name):
        return self.__users[name][1]

    # -------------------------------------------------------------------------
    def dump(self):
        seiscomp.logging.info("known users:")

        for name, user in list(self.__users.items()):
            seiscomp.logging.info(f" {u_str(name)} {user[1]} {user[2]}")


###############################################################################
class Access:
    # -------------------------------------------------------------------------
    def __init__(self):
        self.__access = {}

    # -------------------------------------------------------------------------
    def initFromSC3Routing(self, routing):
        for i in range(routing.accessCount()):
            acc = routing.access(i)
            net = acc.networkCode()
            sta = acc.stationCode()
            loc = acc.locationCode()
            cha = acc.streamCode()
            user = acc.user()
            start = acc.start()

            try:
                end = acc.end()

            except ValueError:
                end = None

            self.__access.setdefault((net, sta, loc, cha), []).append(
                (user, start, end)
            )

    # -------------------------------------------------------------------------
    @staticmethod
    def __matchTime(t1, t2, accessStart, accessEnd):
        return (not accessStart or (t1 and t1 >= accessStart)) and (
            not accessEnd or (t2 and t2 <= accessEnd)
        )

    # -------------------------------------------------------------------------
    @staticmethod
    def __matchEmail(emailAddress, accessUser):
        defaultPrefix = "mail:"

        if accessUser.startswith(defaultPrefix):
            accessUser = accessUser[len(defaultPrefix) :]

        return emailAddress.upper() == accessUser.upper() or (
            accessUser[:1] == "@"
            and emailAddress[:1] != "@"
            and emailAddress.upper().endswith(accessUser.upper())
        )

    # -------------------------------------------------------------------------
    @staticmethod
    def __matchAttribute(attribute, accessUser):
        return attribute.upper() == accessUser.upper()

    # -------------------------------------------------------------------------
    def authorize(self, user, net, sta, loc, cha, t1, t2):
        if user["blacklisted"]:
            return False

        matchers = []

        try:
            # OID 0.9.2342.19200300.100.1.3 (RFC 2798)
            emailAddress = user["mail"]
            matchers.append((self.__matchEmail, emailAddress))

        except KeyError:
            pass

        try:
            # B2ACCESS
            for memberof in user["memberof"].split(";"):
                matchers.append((self.__matchAttribute, f"group:{memberof}"))

        except KeyError:
            pass

        for m in matchers:
            for u, start, end in self.__access.get((net, "", "", ""), []):
                if self.__matchTime(t1, t2, start, end) and m[0](m[1], u):
                    return True

            for u, start, end in self.__access.get((net, sta, "", ""), []):
                if self.__matchTime(t1, t2, start, end) and m[0](m[1], u):
                    return True

            for u, start, end in self.__access.get((net, sta, loc, cha), []):
                if self.__matchTime(t1, t2, start, end) and m[0](m[1], u):
                    return True

        return False


###############################################################################
class DataAvailabilityCache:
    # -------------------------------------------------------------------------
    def __init__(self, app, da, validUntil):
        self._da = da
        self._validUntil = validUntil
        self._extents = {}
        self._extentsSorted = []
        self._extentsOID = {}

        for i in range(self._da.dataExtentCount()):
            ext = self._da.dataExtent(i)
            wid = ext.waveformID()
            sid = (
                f"{wid.networkCode()}.{wid.stationCode()}.{wid.locationCode()}."
                f"{wid.channelCode()}"
            )
            restricted = app._openStreams is None or sid not in app._openStreams
            if restricted and not app._allowRestricted:
                continue

            self._extents[sid] = (ext, restricted)
            # seiscomp.logging.debug("%s: %s ~ %s" % (sid, ext.start().iso(),
            #                               ext.end().iso()))

        if app._serveAvailability:
            # load data attribute extents if availability is served
            for i in range(da.dataExtentCount()):
                extent = da.dataExtent(i)
                app.query().loadDataAttributeExtents(extent)

            # create a list of (extent, oid, restricted) tuples sorted by stream
            self._extentsSorted = [
                (e, app.query().getCachedId(e), res)
                for wid, (e, res) in sorted(self._extents.items(), key=lambda t: t[0])
            ]

            # create a dictionary of object ID to extents
            self._extentsOID = dict(
                (oid, (e, res)) for (e, oid, res) in self._extentsSorted
            )

        seiscomp.logging.info(f"loaded {len(self._extents)} extents")

    # -------------------------------------------------------------------------
    def validUntil(self):
        return self._validUntil

    # -------------------------------------------------------------------------
    def extent(self, net, sta, loc, cha):
        wid = f"{net}.{sta}.{loc}.{cha}"
        if wid in self._extents:
            return self._extents[wid][0]

        return None

    # -------------------------------------------------------------------------
    def extents(self):
        return self._extents

    # -------------------------------------------------------------------------
    def extentsSorted(self):
        return self._extentsSorted

    # -------------------------------------------------------------------------
    def extentsOID(self):
        return self._extentsOID

    # -------------------------------------------------------------------------
    def dataAvailability(self):
        return self._da


###############################################################################
class FDSNWS(seiscomp.client.Application):
    # -------------------------------------------------------------------------
    def __init__(self, argc, argv):
        super().__init__(argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setRecordStreamEnabled(True)
        self.setLoadInventoryEnabled(True)

        self._serverRoot = os.path.dirname(__file__)
        self._listenAddress = "0.0.0.0"  # all interfaces
        self._port = 8080
        self._connections = 5
        self._queryObjects = 100000  # maximum number of objects per query
        self._realtimeGap = None  # minimum data age: 5min
        self._samplesM = None  # maximum number of samples per query
        self._recordBulkSize = 102400  # desired record bulk size
        self._htpasswd = "@CONFIGDIR@/fdsnws.htpasswd"
        self._accessLogFile = ""
        self._requestLogFile = ""
        self._userSalt = ""
        self._corsOrigins = ["*"]

        self._allowRestricted = True
        self._useArclinkAccess = False
        self._serveDataSelect = True
        self._serveEvent = True
        self._serveStation = True
        self._serveAvailability = False
        self._daEnabled = False
        self._daCacheDuration = 300
        self._daCache = None
        self._openStreams = None
        self._daRepositoryName = "primary"
        self._daDCCName = "DCC"
        self._handleConditionalRequests = False

        self._hideAuthor = False
        self._hideComments = False
        self._evaluationMode = None
        self._eventTypeWhitelist = None
        self._eventTypeBlacklist = None
        self._eventFormats = None
        self._stationFilter = None
        self._invCoordinatePrecision = None
        self._dataSelectFilter = None
        self._debugFilter = False

        self._accessLog = None

        self._fileNamePrefix = "fdsnws"

        self._trackdbEnabled = False
        self._trackdbDefaultUser = "fdsnws"

        self._authEnabled = False
        self._authGnupgHome = "@ROOTDIR@/var/lib/gpg"
        self._authBlacklist = []

        self._userdb = UserDB()
        self._access = None
        self._checker = None

        self._requestLog = None
        self.__reloadRequested = False
        self.__timeInventoryLoaded = None
        self.__tcpPort = None

        # Leave signal handling to us
        seiscomp.client.Application.HandleSignals(False, False)

    # -------------------------------------------------------------------------
    def initConfiguration(self):
        if not seiscomp.client.Application.initConfiguration(self):
            return False

        # bind address and port
        try:
            self._listenAddress = self.configGetString("listenAddress")
        except Exception:
            pass

        try:
            self._port = self.configGetInt("port")
        except Exception:
            pass

        # maximum number of connections
        try:
            self._connections = self.configGetInt("connections")
        except Exception:
            pass

        # maximum number of objects per query, used in fdsnws-station and
        # fdsnws-event to limit main memory consumption
        try:
            self._queryObjects = self.configGetInt("queryObjects")
        except Exception:
            pass

        # restrict end time of request to now-realtimeGap seconds, used in
        # fdsnws-dataselect
        try:
            self._realtimeGap = self.configGetInt("realtimeGap")
        except Exception:
            pass

        # maximum number of samples (in units of million) per query, used in
        # fdsnws-dataselect to limit bandwidth
        try:
            self._samplesM = self.configGetDouble("samplesM")
        except Exception:
            pass

        try:
            self._recordBulkSize = self.configGetInt("recordBulkSize")
        except Exception:
            pass

        if self._recordBulkSize < 1:
            print("Invalid recordBulkSize, must be larger than 0", file=sys.stderr)
            return False

        # location of htpasswd file
        try:
            self._htpasswd = self.configGetString("htpasswd")
        except Exception:
            pass
        self._htpasswd = seiscomp.system.Environment.Instance().absolutePath(
            self._htpasswd
        )

        # location of access log file
        try:
            self._accessLogFile = seiscomp.system.Environment.Instance().absolutePath(
                self.configGetString("accessLog")
            )
        except Exception:
            pass

        # location of request log file
        try:
            self._requestLogFile = seiscomp.system.Environment.Instance().absolutePath(
                self.configGetString("requestLog")
            )
        except Exception:
            pass

        # user salt
        try:
            self._userSalt = self.configGetString("userSalt")
        except Exception:
            pass

        # list of allowed CORS origins
        try:
            self._corsOrigins = list(filter(None, self.configGetStrings("corsOrigins")))
        except Exception:
            pass

        # access to restricted inventory information
        try:
            self._allowRestricted = self.configGetBool("allowRestricted")
        except Exception:
            pass

        # time-based conditional requests handled by fdsnws-station
        try:
            self._handleConditionalRequests = self.configGetBool(
                "handleConditionalRequests"
            )
        except Exception:
            pass

        # use arclink-access bindings
        try:
            self._useArclinkAccess = self.configGetBool("useArclinkAccess")
        except Exception:
            pass

        # services to enable
        try:
            self._serveDataSelect = self.configGetBool("serveDataSelect")
        except Exception:
            pass

        try:
            self._serveEvent = self.configGetBool("serveEvent")
        except Exception:
            pass

        try:
            self._serveStation = self.configGetBool("serveStation")
        except Exception:
            pass

        try:
            self._serveAvailability = self.configGetBool("serveAvailability")
        except Exception:
            pass

        # data availability
        try:
            self._daEnabled = self.configGetBool("dataAvailability.enable")
        except Exception:
            pass

        try:
            self._daCacheDuration = self.configGetInt("dataAvailability.cacheDuration")
        except Exception:
            pass

        try:
            self._daRepositoryName = self.configGetString(
                "dataAvailability.repositoryName"
            )
        except Exception:
            pass

        try:
            self._daDCCName = self.configGetString("dataAvailability.dccName")
        except Exception:
            pass

        if self._serveAvailability and not self._daEnabled:
            print(
                "can't serve availabilty without dataAvailability.enable "
                "set to true",
                file=sys.stderr,
            )
            return False

        if not bool(re.match(r"^[a-zA-Z0-9_\ -]*$", self._daRepositoryName)):
            print(
                "invalid characters in dataAvailability.repositoryName", file=sys.stderr
            )
            return False

        if not bool(re.match(r"^[a-zA-Z0-9_\ -]*$", self._daDCCName)):
            print("invalid characters in dataAvailability.dccName", file=sys.stderr)
            return False

        # event filter
        try:
            self._hideAuthor = self.configGetBool("hideAuthor")
        except Exception:
            pass

        try:
            self._hideComments = self.configGetBool("hideComments")
        except Exception:
            pass

        try:
            name = self.configGetString("evaluationMode")
            if name.lower() == seiscomp.datamodel.EEvaluationModeNames.name(
                seiscomp.datamodel.MANUAL
            ):
                self._evaluationMode = seiscomp.datamodel.MANUAL
            elif name.lower() == seiscomp.datamodel.EEvaluationModeNames.name(
                seiscomp.datamodel.AUTOMATIC
            ):
                self._evaluationMode = seiscomp.datamodel.AUTOMATIC
            else:
                print(f"invalid evaluation mode string: {name}", file=sys.stderr)
                return False

        except Exception:
            pass

        try:
            strings = self.configGetStrings("eventType.whitelist")
            if len(strings) > 1 or strings[0]:
                try:
                    self._eventTypeWhitelist = self._parseEventTypes(strings)
                except Exception as e:
                    print(f"error parsing eventType.whitelist: {e}", file=sys.stderr)
                    return False
        except Exception:
            pass

        try:
            strings = self.configGetStrings("eventType.blacklist")
            if len(strings) > 1 or strings[0]:
                try:
                    self._eventTypeBlacklist = self._parseEventTypes(strings)
                    if self._eventTypeWhitelist:
                        lenBefore = len(self._eventTypeWhitelist)
                        diff = self._eventTypeWhitelist.difference(
                            self._eventTypeBlacklist
                        )
                        overlapCount = lenBefore - len(diff)
                        if overlapCount > 0:
                            self._eventTypeWhitelist = diff
                            print(
                                f"warning: found {overlapCount} overlapping event "
                                "types in white and black list, black list takes "
                                "precedence",
                                file=sys.stderr,
                            )
                except Exception as e:
                    print(f"error parsing eventType.blacklist: {e}", file=sys.stderr)
                    return False

        except Exception:
            pass

        try:
            strings = self.configGetStrings("eventFormats")
            if len(strings) > 1 or strings[0]:
                self._eventFormats = [s.lower() for s in strings]
        except Exception:
            pass

        # station filter
        try:
            self._stationFilter = seiscomp.system.Environment.Instance().absolutePath(
                self.configGetString("stationFilter")
            )
        except Exception:
            pass

        # precision of inventory locations in number of decimal places
        try:
            precision = self.configGetString("inventoryCoordinatePrecision")
            try:
                self._invCoordinatePrecision = int(precision)
                if self._invCoordinatePrecision < 0:
                    raise ValueError
            except ValueError:
                print(
                    "invalid value in inventoryCoorinatePrecision, expected number of "
                    "decimal places of geo coordinate",
                    file=sys.stderr,
                )
                return False

        except Exception:
            pass

        # dataSelect filter
        try:
            self._dataSelectFilter = (
                seiscomp.system.Environment.Instance().absolutePath(
                    self.configGetString("dataSelectFilter")
                )
            )
        except Exception:
            pass

        # output filter debug information
        try:
            self._debugFilter = self.configGetBool("debugFilter")
        except Exception:
            pass

        # prefix to be used as default for output filenames
        try:
            self._fileNamePrefix = self.configGetString("fileNamePrefix")
        except Exception:
            pass

        # save request logs in database?
        try:
            self._trackdbEnabled = self.configGetBool("trackdb.enable")
        except Exception:
            pass

        # default user
        try:
            self._trackdbDefaultUser = self.configGetString("trackdb.defaultUser")
        except Exception:
            pass

        # enable authentication extension?
        try:
            self._authEnabled = self.configGetBool("auth.enable")
        except Exception:
            pass

        # GnuPG home directory
        try:
            self._authGnupgHome = self.configGetString("auth.gnupgHome")
        except Exception:
            pass
        self._authGnupgHome = seiscomp.system.Environment.Instance().absolutePath(
            self._authGnupgHome
        )

        # blacklist of users/tokens
        try:
            strings = self.configGetStrings("auth.blacklist")
            if len(strings) > 1 or strings[0]:
                self._authBlacklist = strings
        except Exception:
            pass

        # If the database connection is passed via command line or
        # configuration file then messaging is disabled. Messaging is only used
        # to get the configured database connection URI.
        if self.databaseURI() != "":
            self.setMessagingEnabled(self._trackdbEnabled)
        else:
            # Without the event service, a database connection is not
            # required if the inventory is loaded from file and no data
            # availability information should be processed
            if (
                not self._serveEvent
                and not self._useArclinkAccess
                and (
                    not self._serveStation
                    or (not self.isInventoryDatabaseEnabled() and not self._daEnabled)
                )
            ):
                self.setMessagingEnabled(self._trackdbEnabled)
                self.setDatabaseEnabled(False, False)

        return True

    def printUsage(self):
        print(
            """Usage:
  fdsnws [options]

Provide FDSN Web Services"""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            """Examples:
Execute on command line with debug output
  fdsnws --debug
"""
        )

    # -------------------------------------------------------------------------
    # Signal handling in Python and fork in wrapped C++ code is not a good
    # combination. Without digging too much into the problem, forking the
    # process with os.fork() helps
    def forkProcess(self):
        cp = os.fork()
        if cp < 0:
            return False
        if cp == 0:
            return True

        sys.exit(0)
        return True  # pylint: disable=unreachable

    # -------------------------------------------------------------------------
    def getDACache(self):
        if not self._daEnabled:
            return None

        now = seiscomp.core.Time.GMT()
        # check if cache is still valid
        if self._daCache is None or now > self._daCache.validUntil():
            if self.query() is None:
                seiscomp.logging.error("failed to connect to database")
                return None

            da = seiscomp.datamodel.DataAvailability()
            self.query().loadDataExtents(da)
            validUntil = now + seiscomp.core.TimeSpan(self._daCacheDuration, 0)
            self._daCache = DataAvailabilityCache(self, da, validUntil)

        return self._daCache

    # -------------------------------------------------------------------------
    @staticmethod
    def _parseEventTypes(names):
        types = set()
        typeMap = {
            seiscomp.datamodel.EEventTypeNames.name(i): i
            for i in range(seiscomp.datamodel.EEventTypeQuantity)
        }
        for n in names:
            name = n.lower().strip()
            if name == "unknown":
                types.add(-1)
            else:
                if name in typeMap:
                    types.add(typeMap[name])
                else:
                    raise ValueError(f"event type name '{name}' not supported")

        return types

    # -------------------------------------------------------------------------
    @staticmethod
    def _formatEventTypes(types):
        return ",".join(
            [
                "unknown" if i < 0 else seiscomp.datamodel.EEventTypeNames.name(i)
                for i in sorted(types)
            ]
        )

    # -------------------------------------------------------------------------
    def _site(self):
        modeStr = None
        if self._evaluationMode is not None:
            modeStr = seiscomp.datamodel.EEvaluationModeNames.name(self._evaluationMode)
        whitelistStr = "<None>"
        if self._eventTypeWhitelist is not None:
            whitelistStr = ", ".join(
                [
                    "unknown" if i < 0 else seiscomp.datamodel.EEventTypeNames.name(i)
                    for i in sorted(self._eventTypeWhitelist)
                ]
            )
        blacklistStr = "<None>"
        if self._eventTypeBlacklist is not None:
            blacklistStr = ", ".join(
                [
                    "unknown" if i < 0 else seiscomp.datamodel.EEventTypeNames.name(i)
                    for i in sorted(self._eventTypeBlacklist)
                ]
            )
        stationFilterStr = "<None>"
        if self._stationFilter is not None:
            stationFilterStr = self._stationFilter
        dataSelectFilterStr = "<None>"
        if self._dataSelectFilter is not None:
            dataSelectFilterStr = self._dataSelectFilter

        if self._invCoordinatePrecision is not None:
            invCoordinatePrecisionStr = (
                f"{self._invCoordinatePrecision} decimal places (≅"
                f"{int(KM_OF_DEGREE * 1000 / 10**self._invCoordinatePrecision)}m)"
            )
        else:
            invCoordinatePrecisionStr = "unlimited"

        seiscomp.logging.debug(
            f"""
configuration read:
  serve
    dataselect             : {self._serveDataSelect}
    event                  : {self._serveEvent}
    station                : {self._serveStation}
    availability           : {self._serveAvailability}
  listenAddress            : {self._listenAddress}
  port                     : {self._port}
  connections              : {self._connections}
  htpasswd                 : {self._htpasswd}
  accessLog                : {self._accessLogFile}
  CORS origins             : {self._corsOrigins}
  queryObjects             : {self._queryObjects}
  realtimeGap              : {self._realtimeGap}
  samples (M)              : {self._samplesM}
  recordBulkSize           : {self._recordBulkSize}
  allowRestricted          : {self._allowRestricted}
  handleConditionalRequests: {self._handleConditionalRequests}
  useArclinkAccess         : {self._useArclinkAccess}
  hideAuthor               : {self._hideAuthor}
  hideComments             : {self._hideComments}
  evaluationMode           : {modeStr}
  data availability
    enabled                : {self._daEnabled}
    cache duration         : {self._daCacheDuration}
    repo name              : {self._daRepositoryName}
    dcc name               : {self._daDCCName}
  eventType
    whitelist              : {whitelistStr}
    blacklist              : {blacklistStr}
  inventory filter
    station                : {stationFilterStr}
    dataSelect             : {dataSelectFilterStr}
    debug enabled          : {self._debugFilter}
    coordinate precision   : {invCoordinatePrecisionStr}
  trackdb
    enabled                : {self._trackdbEnabled}
    defaultUser            : {self._trackdbDefaultUser}
  auth
    enabled                : {self._authEnabled}
    gnupgHome              : {self._authGnupgHome}
  requestLog               : {self._requestLogFile}"""
        )

        if (
            not self._serveDataSelect
            and not self._serveEvent
            and not self._serveStation
        ):
            seiscomp.logging.error("all services disabled through configuration")
            return None

        # access logger if requested
        if self._accessLogFile:
            self._accessLog = Log(self._accessLogFile)

        # load inventory needed by DataSelect and Station service
        stationInv = dataSelectInv = None
        if self._serveDataSelect or self._serveStation:
            retn = False
            stationInv = (
                dataSelectInv
            ) = seiscomp.client.Inventory.Instance().inventory()
            seiscomp.logging.info("inventory loaded")

            if self._serveDataSelect and self._serveStation:
                # clone inventory if station and dataSelect filter are distinct
                # else share inventory between both services
                if self._stationFilter != self._dataSelectFilter:
                    dataSelectInv = self._cloneInventory(stationInv)
                    retn = self._filterInventory(
                        stationInv, self._stationFilter, "station"
                    ) and self._filterInventory(
                        dataSelectInv, self._dataSelectFilter, "dataSelect"
                    )
                else:
                    retn = self._filterInventory(stationInv, self._stationFilter)
            elif self._serveStation:
                retn = self._filterInventory(stationInv, self._stationFilter)
            else:
                retn = self._filterInventory(dataSelectInv, self._dataSelectFilter)

            self._obfuscateInventory(stationInv)
            if stationInv != dataSelectInv:
                self._obfuscateInventory(dataSelectInv)

            self.__timeInventoryLoaded = seiscomp.core.Time.GMT()

            if not retn:
                return None

        if self._authEnabled:
            self._access = Access()
            self._checker = UsernamePasswordChecker(self._userdb)
        else:
            self._access = Access() if self._useArclinkAccess else None
            self._checker = checkers.FilePasswordDB(self._htpasswd, cache=True)

        if self._serveDataSelect and self._useArclinkAccess:
            self._access.initFromSC3Routing(self.query().loadRouting())

        seiscomp.datamodel.PublicObject.SetRegistrationEnabled(False)

        shareDir = os.path.join(
            seiscomp.system.Environment.Instance().shareDir(), "fdsnws"
        )

        # Overwrite/set mime type of *.wadl and *.xml documents. Instead of
        # using the official types defined in /etc/mime.types 'application/xml'
        # is used as enforced by the FDSNWS spec.
        static.File.contentTypes[".wadl"] = "application/xml"
        static.File.contentTypes[".xml"] = "application/xml"

        # create resource tree /fdsnws/...
        root = ListingResource()

        fileName = os.path.join(shareDir, "favicon.ico")
        fileRes = static.File(fileName, "image/x-icon")
        fileRes.childNotFound = NoResource()
        fileRes.isLeaf = True
        root.putChild(b"favicon.ico", fileRes)

        prefix = ListingResource()
        root.putChild(b"fdsnws", prefix)

        # dataselect
        if self._serveDataSelect:
            dataselect = ListingResource(DataSelectVersion)
            prefix.putChild(b"dataselect", dataselect)
            lstFile = os.path.join(shareDir, "dataselect.html")
            dataselect1 = DirectoryResource(lstFile, DataSelectVersion)
            dataselect.putChild(b"1", dataselect1)

            # query
            dataselect1.putChild(
                b"query", FDSNDataSelect(dataSelectInv, self._recordBulkSize)
            )

            # queryauth
            if self._authEnabled:
                realm = FDSNDataSelectAuthRealm(
                    dataSelectInv, self._recordBulkSize, self._access, self._userdb
                )
            else:
                realm = FDSNDataSelectRealm(
                    dataSelectInv, self._recordBulkSize, self._access
                )
            msg = "authorization for restricted time series data required"
            authSession = self._getAuthSessionWrapper(realm, msg)
            dataselect1.putChild(b"queryauth", authSession)

            # version
            dataselect1.putChild(b"version", ServiceVersion(DataSelectVersion))
            fileRes = static.File(os.path.join(shareDir, "dataselect.wadl"))
            fileRes.childNotFound = NoResource(DataSelectVersion)

            # application.wadl
            dataselect1.putChild(b"application.wadl", fileRes)

            # builder
            fileRes = static.File(os.path.join(shareDir, "dataselect-builder.html"))
            fileRes.childNotFound = NoResource(DataSelectVersion)
            dataselect1.putChild(b"builder", fileRes)

            if self._authEnabled:
                dataselect1.putChild(
                    b"auth",
                    AuthResource(DataSelectVersion, self._authGnupgHome, self._userdb),
                )

        # event
        if self._serveEvent:
            event = ListingResource(EventVersion)
            prefix.putChild(b"event", event)
            lstFile = os.path.join(shareDir, "event.html")
            event1 = DirectoryResource(lstFile, EventVersion)
            event.putChild(b"1", event1)

            # query
            event1.putChild(
                b"query",
                FDSNEvent(
                    self._hideAuthor,
                    self._hideComments,
                    self._evaluationMode,
                    self._eventTypeWhitelist,
                    self._eventTypeBlacklist,
                    self._eventFormats,
                ),
            )

            # catalogs
            fileRes = static.File(os.path.join(shareDir, "catalogs.xml"))
            fileRes.childNotFound = NoResource(EventVersion)
            event1.putChild(b"catalogs", fileRes)

            # contributors
            fileRes = static.File(os.path.join(shareDir, "contributors.xml"))
            fileRes.childNotFound = NoResource(EventVersion)
            event1.putChild(b"contributors", fileRes)

            # version
            event1.putChild(b"version", ServiceVersion(EventVersion))

            # application.wadl
            filterList = ["includecomments"] if self._hideComments else []
            try:
                fileRes = WADLFilter(os.path.join(shareDir, "event.wadl"), filterList)
            except Exception:
                fileRes = NoResource(StationVersion)
            event1.putChild(b"application.wadl", fileRes)

            # builder
            fileRes = static.File(os.path.join(shareDir, "event-builder.html"))
            fileRes.childNotFound = NoResource(EventVersion)
            event1.putChild(b"builder", fileRes)

        # station
        if self._serveStation:
            station = ListingResource(StationVersion)
            prefix.putChild(b"station", station)
            lstFile = os.path.join(shareDir, "station.html")
            station1 = DirectoryResource(lstFile, StationVersion)
            station.putChild(b"1", station1)

            # query
            station1.putChild(
                b"query",
                FDSNStation(
                    stationInv,
                    self._allowRestricted,
                    self._queryObjects,
                    self._daEnabled,
                    self._handleConditionalRequests,
                    self.__timeInventoryLoaded,
                ),
            )

            # version
            station1.putChild(b"version", ServiceVersion(StationVersion))

            # application.wadl
            filterList = [] if self._daEnabled else ["matchtimeseries"]
            try:
                fileRes = WADLFilter(os.path.join(shareDir, "station.wadl"), filterList)
            except Exception:
                fileRes = NoResource(StationVersion)
            station1.putChild(b"application.wadl", fileRes)

            # builder
            fileRes = static.File(os.path.join(shareDir, "station-builder.html"))
            fileRes.childNotFound = NoResource(StationVersion)
            station1.putChild(b"builder", fileRes)

        # availability
        if self._serveAvailability:
            # create a set of waveformIDs which represent open channels
            if self._serveDataSelect:
                openStreams = set()

                for iNet in range(dataSelectInv.networkCount()):
                    net = dataSelectInv.network(iNet)
                    if isRestricted(net):
                        continue

                    for iSta in range(net.stationCount()):
                        sta = net.station(iSta)
                        if isRestricted(sta):
                            continue

                        for iLoc in range(sta.sensorLocationCount()):
                            loc = sta.sensorLocation(iLoc)

                            for iCha in range(loc.streamCount()):
                                cha = loc.stream(iCha)
                                if isRestricted(cha):
                                    continue

                                openStreams.add(
                                    f"{net.code()}.{sta.code()}.{loc.code()}."
                                    f"{cha.code()}"
                                )

                self._openStreams = openStreams
            else:
                self._openStreams = None

            availability = ListingResource(AvailabilityVersion)
            prefix.putChild(b"availability", availability)
            lstFile = os.path.join(shareDir, "availability.html")
            availability1 = DirectoryResource(lstFile, AvailabilityVersion)
            availability.putChild(b"1", availability1)

            # query
            availability1.putChild(b"query", FDSNAvailabilityQuery())

            # queryauth
            if self._authEnabled:
                realm = FDSNAvailabilityQueryAuthRealm(self._access, self._userdb)
            else:
                realm = FDSNAvailabilityQueryRealm(self._access)
            msg = "authorization for restricted availability segment data required"
            authSession = self._getAuthSessionWrapper(realm, msg)
            availability1.putChild(b"queryauth", authSession)

            # extent
            availability1.putChild(b"extent", FDSNAvailabilityExtent())

            # extentauth
            if self._authEnabled:
                realm = FDSNAvailabilityExtentAuthRealm(self._access, self._userdb)
            else:
                realm = FDSNAvailabilityExtentRealm(self._access)
            msg = "authorization for restricted availability extent data required"
            authSession = self._getAuthSessionWrapper(realm, msg)
            availability1.putChild(b"extentauth", authSession)

            # version
            availability1.putChild(b"version", ServiceVersion(AvailabilityVersion))

            # application.wadl
            fileRes = static.File(os.path.join(shareDir, "availability.wadl"))
            fileRes.childNotFound = NoResource(AvailabilityVersion)
            availability1.putChild(b"application.wadl", fileRes)

            # builder-query
            fileRes = static.File(
                os.path.join(shareDir, "availability-builder-query.html")
            )
            fileRes.childNotFound = NoResource(AvailabilityVersion)
            availability1.putChild(b"builder-query", fileRes)

            # builder-extent
            fileRes = static.File(
                os.path.join(shareDir, "availability-builder-extent.html")
            )
            fileRes.childNotFound = NoResource(AvailabilityVersion)
            availability1.putChild(b"builder-extent", fileRes)

        # static files
        fileRes = static.File(os.path.join(shareDir, "js"))
        fileRes.childNotFound = NoResource()
        fileRes.hideInListing = True
        prefix.putChild(b"js", fileRes)

        fileRes = static.File(os.path.join(shareDir, "css"))
        fileRes.childNotFound = NoResource()
        fileRes.hideInListing = True
        prefix.putChild(b"css", fileRes)

        return Site(root, self._corsOrigins)

    # -------------------------------------------------------------------------
    def _reloadTask(self):
        if not self.__reloadRequested:
            return

        seiscomp.logging.info("reloading inventory")
        self.reloadInventory()

        site = self._site()

        if site:
            self.__tcpPort.factory = site

            # remove reload file
            try:
                reloadfile = os.path.join(
                    seiscomp.system.Environment.Instance().installDir(),
                    "var",
                    "run",
                    f"{self.name()}.reload",
                )
                if os.path.isfile(reloadfile):
                    os.remove(reloadfile)

            except Exception as e:
                seiscomp.logging.warning(f"error processing reload file: {e}")

            seiscomp.logging.info("reload successful")

        else:
            seiscomp.logging.info("reload failed")

        self._userdb.dump()
        self.__reloadRequested = False

    # -------------------------------------------------------------------------
    def _sighupHandler(self, signum, frame):  # pylint: disable=W0613
        if self.__reloadRequested:
            seiscomp.logging.info("SIGHUP received, reload already in progress")
        else:
            seiscomp.logging.info("SIGHUP received, reload scheduled")
            self.__reloadRequested = True

    # -------------------------------------------------------------------------
    def run(self):
        retn = False
        try:
            # request logger if requested
            self._requestLog = None
            if self._requestLogFile:
                # import here, so we don't depend on GeoIP if request log is not
                # needed
                from seiscomp.fdsnws.reqlog import RequestLog  # pylint: disable=C0415

                self._requestLog = RequestLog(self._requestLogFile, self._userSalt)

            for user in self._authBlacklist:
                self._userdb.blacklistUser(user)

            site = self._site()

            if not site:
                return False

            # start listen for incoming request
            self.__tcpPort = reactor.listenTCP(
                self._port, site, self._connections, self._listenAddress
            )

            # setup signal handler
            signal.signal(signal.SIGHUP, self._sighupHandler)
            task.LoopingCall(self._reloadTask).start(1, False)

            # start processing
            seiscomp.logging.info("start listening")
            log.addObserver(logSC3)

            reactor.run()
            retn = True
        except Exception as e:
            seiscomp.logging.error(str(e))

        return retn

    # -------------------------------------------------------------------------
    @staticmethod
    def _cloneInventory(inv):
        wasEnabled = seiscomp.datamodel.PublicObject.IsRegistrationEnabled()
        seiscomp.datamodel.PublicObject.SetRegistrationEnabled(False)
        inv2 = seiscomp.datamodel.Inventory.Cast(inv.clone())

        for iNet in range(inv.networkCount()):
            net = inv.network(iNet)
            net2 = seiscomp.datamodel.Network.Cast(net.clone())
            inv2.add(net2)

            for iSta in range(net.stationCount()):
                sta = net.station(iSta)
                sta2 = seiscomp.datamodel.Station.Cast(sta.clone())
                net2.add(sta2)

                for iLoc in range(sta.sensorLocationCount()):
                    loc = sta.sensorLocation(iLoc)
                    loc2 = seiscomp.datamodel.SensorLocation.Cast(loc.clone())
                    sta2.add(loc2)

                    for iCha in range(loc.streamCount()):
                        cha = loc.stream(iCha)
                        cha2 = seiscomp.datamodel.Stream.Cast(cha.clone())
                        loc2.add(cha2)

        seiscomp.datamodel.PublicObject.SetRegistrationEnabled(wasEnabled)
        return inv2

    # -------------------------------------------------------------------------
    def _filterInventory(self, inv, fileName, serviceName=""):
        if not fileName:
            return True

        class FilterRule:
            def __init__(self, name, code):
                self.name = name
                self.exclude = name.startswith("!")
                self.code = code

                self.restricted = None
                self.shared = None
                self.netClass = None
                self.archive = None

        # read filter configuration from INI file
        invFilter = []
        includeRuleDefined = False
        try:
            from configparser import ConfigParser
            from configparser import Error as CPError
        except ImportError:
            seiscomp.logging.error("could not load 'ConfigParser' Python module")
            return False

        cp = ConfigParser()

        try:
            seiscomp.logging.notice(f"reading inventory filter file: {fileName}")
            with open(fileName, "r", encoding="utf-8") as fp:
                cp.read_file(fp, fileName)

            if len(cp.sections()) == 0:
                return True

            # check for mandatory code attribute
            for sectionName in cp.sections():
                code = ""
                try:
                    code = cp.get(sectionName, "code")
                except CPError:
                    seiscomp.logging.error(
                        f"missing 'code' attribute in section {sectionName} of "
                        f"inventory filter file {fileName}"
                    )
                    return False

                rule = FilterRule(sectionName, str(code))

                try:
                    rule.restricted = cp.getboolean(sectionName, "restricted")
                except CPError:
                    pass

                try:
                    rule.shared = cp.getboolean(sectionName, "shared")
                except CPError:
                    pass

                try:
                    rule.netClass = str(cp.get(sectionName, "netClass"))
                except CPError:
                    pass

                try:
                    rule.archive = str(cp.get(sectionName, "archive"))
                except CPError:
                    pass

                includeRuleDefined |= not rule.exclude
                invFilter.append(rule)

        except Exception as e:
            seiscomp.logging.error(
                f"could not read inventory filter file {fileName}: {e}"
            )
            return False

        # apply filter
        # networks
        if self._debugFilter:
            debugLines = []
        delNet = delSta = delLoc = delCha = 0
        iNet = 0
        while iNet < inv.networkCount():
            net = inv.network(iNet)

            try:
                netRestricted = net.restricted()
            except ValueError:
                netRestricted = None
            try:
                netShared = net.shared()
            except ValueError:
                netShared = None

            # stations
            iSta = 0
            while iSta < net.stationCount():
                sta = net.station(iSta)
                staCode = f"{net.code()}.{sta.code()}"

                try:
                    staRestricted = sta.restricted()
                except ValueError:
                    staRestricted = None
                try:
                    staShared = sta.shared()
                except ValueError:
                    staShared = None

                # sensor locations
                iLoc = 0
                while iLoc < sta.sensorLocationCount():
                    loc = sta.sensorLocation(iLoc)
                    locCode = f"{staCode}.{loc.code()}"

                    # channels
                    iCha = 0
                    while iCha < loc.streamCount():
                        cha = loc.stream(iCha)
                        code = f"{locCode}.{cha.code()}"

                        # evaluate rules until matching code is found
                        match = False
                        for rule in invFilter:
                            # code
                            if not fnmatch.fnmatchcase(code, rule.code):
                                continue

                            # restricted
                            if rule.restricted is not None:
                                try:
                                    if cha.restricted() != rule.restricted:
                                        continue
                                except ValueError:
                                    if staRestricted is not None:
                                        if staRestricted != rule.restricted:
                                            continue
                                    elif (
                                        netRestricted is None
                                        or netRestricted != rule.restricted
                                    ):
                                        continue

                            # shared
                            if rule.shared is not None:
                                try:
                                    if cha.shared() != rule.shared:
                                        continue
                                except ValueError:
                                    if staShared is not None:
                                        if staShared != rule.shared:
                                            continue
                                    elif netShared is None or netShared != rule.shared:
                                        continue

                            # netClass
                            if (
                                rule.netClass is not None
                                and net.netClass() != rule.netClass
                            ):
                                continue

                            # archive
                            if (
                                rule.archive is not None
                                and net.archive() != rule.archive
                            ):
                                continue

                            # the rule matched
                            match = True
                            break

                        if (match and rule.exclude) or (
                            not match and includeRuleDefined
                        ):
                            loc.removeStream(iCha)
                            delCha += 1
                            reason = "no matching include rule"
                            if match:
                                reason = f"'{rule.name}'"
                            if self._debugFilter:
                                debugLines.append(f"{code} [-]: {reason}")
                        else:
                            iCha += 1
                            reason = "no matching exclude rule"
                            if match:
                                reason = f"'{rule.name}'"
                            if self._debugFilter:
                                debugLines.append(f"{code} [+]: {reason}")

                    # remove empty sensor locations
                    if loc.streamCount() == 0:
                        sta.removeSensorLocation(iLoc)
                        delLoc += 1
                    else:
                        iLoc += 1

                # remove empty stations
                if sta.sensorLocationCount() == 0:
                    delSta += 1
                    net.removeStation(iSta)
                else:
                    iSta += 1

            # remove empty networks
            if net.stationCount() == 0:
                delNet += 1
                inv.removeNetwork(iNet)
            else:
                iNet += 1

        if serviceName:
            serviceName += ": "

        seiscomp.logging.debug(
            f"{serviceName}removed {delNet} networks, {delSta} stations, "
            f"{delLoc} locations, {delCha} streams"
        )
        if self._debugFilter:
            debugLines.sort()
            lines = "\n".join(debugLines)
            seiscomp.logging.notice(
                f"{serviceName}filter decisions based on file {fileName}:\n{lines}"
            )

        return True

    # -------------------------------------------------------------------------
    def _obfuscateInventory(self, inv):  # networks
        if self._invCoordinatePrecision is None:
            return

        for iNet in range(inv.networkCount()):
            net = inv.network(iNet)

            # stations
            for iSta in range(net.stationCount()):
                sta = net.station(iSta)
                sta.setLatitude(round(sta.latitude(), self._invCoordinatePrecision))
                sta.setLongitude(round(sta.longitude(), self._invCoordinatePrecision))

                # sensor locations
                for iLoc in range(sta.sensorLocationCount()):
                    loc = sta.sensorLocation(iLoc)
                    loc.setLatitude(round(loc.latitude(), self._invCoordinatePrecision))
                    loc.setLongitude(
                        round(loc.longitude(), self._invCoordinatePrecision)
                    )

    # -------------------------------------------------------------------------
    def _getAuthSessionWrapper(self, realm, msg):
        p = portal.Portal(realm, [self._checker])
        f = guard.DigestCredentialFactory("MD5", msg)
        f.digest = credentials.DigestCredentialFactory("MD5", b_str(msg))
        return HTTPAuthSessionWrapper(p, [f])


fdsnwsApp = FDSNWS(len(sys.argv), sys.argv)
sys.exit(fdsnwsApp())


# vim: ts=4 et tw=79
