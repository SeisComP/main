################################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# HTTP -- Utility methods which generate HTTP result strings
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
################################################################################

import base64
import datetime
import hashlib
import json
import time
import dateutil.parser

from twisted.web import http, resource, server, static, util

import seiscomp.core
import seiscomp.logging

from . import gnupg
from .utils import accessLog, b_str, u_str, writeTSBin

VERSION = "1.2.4"

################################################################################


class HTTP:
    # ---------------------------------------------------------------------------
    @staticmethod
    def renderErrorPage(request, code, msg, version=VERSION, ro=None):
        resp = b"""\
Error %i: %s

%s

Usage details are available from %s

Request:
%s

Request Submitted:
%s

Service Version:
%s
"""

        noContent = code == http.NO_CONTENT

        # rewrite response code if requested and no data was found
        if noContent and ro is not None:
            code = ro.noData

        # set response code
        request.setResponseCode(code)

        # status code 204 requires no message body
        if code == http.NO_CONTENT:
            response = b""
        else:
            request.setHeader("Content-Type", "text/plain")

            reference = b"%s/" % request.path.rpartition(b"/")[0]

            codeStr = http.RESPONSES[code]
            date = b_str(seiscomp.core.Time.GMT().toString("%FT%T.%f"))
            response = resp % (
                code,
                codeStr,
                b_str(msg),
                reference,
                request.uri,
                date,
                b_str(version),
            )
            if not noContent:
                seiscomp.logging.warning(
                    f"responding with error: {code} ({u_str(codeStr)})"
                )

        accessLog(request, ro, code, len(response), msg)
        return response

    # ---------------------------------------------------------------------------
    @staticmethod
    def renderNotFound(request, version=VERSION):
        msg = "The requested resource does not exist on this server."
        return HTTP.renderErrorPage(request, http.NOT_FOUND, msg, version)

    # ---------------------------------------------------------------------------
    @staticmethod
    def renderNotModified(request, ro=None):
        code = http.NOT_MODIFIED
        request.setResponseCode(code)
        request.responseHeaders.removeHeader("content-type")
        accessLog(request, ro, code, 0, None)


################################################################################
class ServiceVersion(resource.Resource):
    isLeaf = True

    # ---------------------------------------------------------------------------
    def __init__(self, version):
        super().__init__()

        self.version = version
        self.type = "text/plain"

    # ---------------------------------------------------------------------------
    def render(self, request):
        request.setHeader("content-type", "text/plain")
        return b_str(self.version)


################################################################################
class WADLFilter(static.Data):
    # ---------------------------------------------------------------------------
    def __init__(self, path, paramNameFilterList):
        data = ""
        removeParam = False
        with open(path, "r", encoding="utf-8") as fp:
            for line in fp:
                lineStripped = line.strip().replace(" ", "")
                if removeParam:
                    if "</param>" in lineStripped:
                        removeParam = False
                    continue

                valid = True
                if "<param" in lineStripped:
                    for f in paramNameFilterList:
                        if f'name="{f}"' in lineStripped:
                            valid = False
                            if lineStripped[-2:] != "/>":
                                removeParam = True
                            break

                if valid:
                    data += line

        super().__init__(b_str(data), "application/xml")


################################################################################
class BaseResource(resource.Resource):
    # ---------------------------------------------------------------------------
    def __init__(self, version=VERSION):
        super().__init__()

        self.version = version

    # ---------------------------------------------------------------------------
    def renderErrorPage(self, request, code, msg, ro=None):
        return HTTP.renderErrorPage(request, code, msg, self.version, ro)

    # ---------------------------------------------------------------------------
    def writeErrorPage(self, request, code, msg, ro=None):
        data = self.renderErrorPage(request, code, msg, ro)
        if data:
            writeTSBin(request, data)

    # ---------------------------------------------------------------------------
    def returnNotModified(self, request, ro=None):
        HTTP.renderNotModified(request, ro)

    # ---------------------------------------------------------------------------
    # Renders error page if the result set exceeds the configured maximum number
    # objects
    def checkObjects(self, request, objCount, maxObj):
        if objCount <= maxObj:
            return True

        msg = (
            "The result set of your request exceeds the configured maximum "
            f"number of objects ({maxObj}). Refine your request parameters."
        )
        self.writeErrorPage(request, http.REQUEST_ENTITY_TOO_LARGE, msg)
        return False


################################################################################
class NoResource(BaseResource):
    isLeaf = True

    # ---------------------------------------------------------------------------
    def render(self, request):
        return HTTP.renderNotFound(request, self.version)

    # ---------------------------------------------------------------------------
    def getChild(self, _path, _request):
        return self


################################################################################
class ListingResource(BaseResource):
    html = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="author" content="gempa GmbH">
  <title>SeisComP FDSNWS Implementation</title>
</head>
<body>
  <p><a href="../">Parent Directory</a></p>
  <h1>SeisComP FDSNWS Web Service</h1>
  <p>Index of %s</p>
  <ul>
%s
  </ul>
</body>"""

    # ---------------------------------------------------------------------------
    def render(self, request):
        lis = ""
        if request.path[-1:] != b"/":
            return util.redirectTo(request.path + b"/", request)

        for k, v in self.children.items():
            if v.isLeaf:
                continue
            if hasattr(v, "hideInListing") and v.hideInListing:
                continue
            name = u_str(k)
            lis += f'<li><a href="{name}/">{name}/</a></li>\n'

        return b_str(ListingResource.html % (u_str(request.path), lis))

    # ---------------------------------------------------------------------------
    def getChild(self, path, _request):
        if not path:
            return self

        return NoResource(self.version)


################################################################################
class DirectoryResource(static.File):
    # ---------------------------------------------------------------------------
    def __init__(self, fileName, version=VERSION):
        super().__init__(fileName)

        self.version = version
        self.childNotFound = NoResource(self.version)

    # ---------------------------------------------------------------------------
    def render(self, request):
        if request.path[-1:] != b"/":
            return util.redirectTo(request.path + b"/", request)

        return static.File.render(self, request)

    # ---------------------------------------------------------------------------
    def getChild(self, path, _request):
        if not path:
            return self

        return NoResource(self.version)


################################################################################
class AuthResource(BaseResource):
    isLeaf = True

    def __init__(self, version, gnupghome, userdb):
        super().__init__(version)

        self.__gpg = gnupg.GPG(gnupghome=gnupghome)
        self.__userdb = userdb

    # ---------------------------------------------------------------------------
    def render_POST(self, request):
        request.setHeader("Content-Type", "text/plain")

        try:
            verified = self.__gpg.decrypt(request.content.getvalue())

        except OSError as e:
            msg = "gpg decrypt error"
            seiscomp.logging.warning(f"{msg}: {e}")
            return self.renderErrorPage(request, http.INTERNAL_SERVER_ERROR, msg)

        except Exception as e:
            msg = "invalid token"
            seiscomp.logging.warning(f"{msg}: {e}")
            return self.renderErrorPage(request, http.BAD_REQUEST, msg)

        if verified.trust_level is None or verified.trust_level < verified.TRUST_FULLY:
            msg = "token has invalid signature"
            seiscomp.logging.warning(msg)
            return self.renderErrorPage(request, http.BAD_REQUEST, msg)

        try:
            attributes = json.loads(u_str(verified.data))
            td = dateutil.parser.parse(
                attributes["valid_until"]
            ) - datetime.datetime.now(dateutil.tz.tzutc())
            lifetime = td.seconds + td.days * 24 * 3600

        except Exception as e:
            msg = "token has invalid validity"
            seiscomp.logging.warning(f"{msg}: {e}")
            return self.renderErrorPage(request, http.BAD_REQUEST, msg)

        if lifetime <= 0:
            msg = "token is expired"
            seiscomp.logging.warning(msg)
            return self.renderErrorPage(request, http.BAD_REQUEST, msg)

        userid = base64.urlsafe_b64encode(hashlib.sha256(verified.data).digest()[:18])
        password = self.__userdb.addUser(
            u_str(userid),
            attributes,
            time.time() + min(lifetime, 24 * 3600),
            u_str(verified.data),
        )
        accessLog(request, None, http.OK, len(userid) + len(password) + 1, None)
        return userid + b":" + password


################################################################################
class Site(server.Site):
    def __init__(self, res, corsOrigins):
        super().__init__(res)

        self._corsOrigins = corsOrigins

    # ---------------------------------------------------------------------------
    def getResourceFor(self, request):
        seiscomp.logging.debug(
            f"request ({request.getClientIP()}): {u_str(request.uri)}"
        )
        request.setHeader("Server", f"SeisComP-FDSNWS/{VERSION}")
        request.setHeader("Access-Control-Allow-Headers", "Authorization")
        request.setHeader("Access-Control-Expose-Headers", "WWW-Authenticate")

        self.setAllowOrigin(request)

        return server.Site.getResourceFor(self, request)

    # ---------------------------------------------------------------------------
    def setAllowOrigin(self, req):
        # no allowed origin: no response header
        lenOrigins = len(self._corsOrigins)
        if lenOrigins == 0:
            return

        # one origin: add header
        if lenOrigins == 1:
            req.setHeader("Access-Control-Allow-Origin", self._corsOrigins[0])
            return

        # more than one origin: check current origin against allowed origins
        # and return the current origin on match.
        origin = req.getHeader("Origin")
        if origin in self._corsOrigins:
            req.setHeader("Access-Control-Allow-Origin", origin)

            # Set Vary header to let the browser know that the response depends
            # on the request. Certain cache strategies should be disabled.
            req.setHeader("Vary", "Origin")


# vim: ts=4 et
