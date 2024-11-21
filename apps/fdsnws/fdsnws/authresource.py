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

from twisted.web import http

import gnupg

import seiscomp.logging

from .utils import accessLog, u_str

from .http import BaseResource


################################################################################
class AuthResource(BaseResource):
    isLeaf = True

    def __init__(self, version, gnupghome, userdb):
        super().__init__(version)

        self.__gpg = gnupg.GPG(gnupghome=gnupghome)
        self.__userdb = userdb

    # ---------------------------------------------------------------------------
    def render_POST(self, request):
        request.setHeader("Content-Type", "text/plain; charset=utf-8")

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
