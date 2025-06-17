################################################################################
# Copyright (C) 2025 GFZ Helmholtz Centre for Geosciences
#
# JWT-based authentication
################################################################################

import json
import jwt
from twisted.cred.checkers import ICredentialsChecker
from twisted.cred.credentials import ICredentials, IAnonymous
from twisted.cred.error import LoginFailed
from twisted.cred.portal import IRealm, Portal
from twisted.web.guard import HTTPAuthSessionWrapper
from twisted.web.iweb import ICredentialFactory
from twisted.web.resource import IResource
from zope.interface import implementer
from urllib.request import urlopen
from seiscomp import logging


class IBearerToken(ICredentials):
    """
    IBearerToken credentials encapsulate a valid bearer token.

    Parameters:
        token   The bearer token str
        payload The decoded and valid token payload
    """


@implementer(IBearerToken)
class JSONWebToken(object):
    """The BearerToken credential object that will be passed to a Checker"""

    def __init__(self, token, payload):
        self.token = token
        self.payload = payload


@implementer(ICredentialFactory)
class _CredentialFactory(object):

    scheme = b"bearer"

    def __init__(self, issuers, audience, algorithms):
        """
        issuers     list. List of valid issuer URLs
        audience    list. List of valid audiences
        algorithms  list. List of algorithms to check
        """
        self.__issuers = issuers
        self.__audience = audience
        self.__algorithms = algorithms
        self.__signing_keys = {}

        for iss in issuers:
            isscfg = json.loads(urlopen(iss.rstrip('/') + '/.well-known/openid-configuration').read())
            aux = json.loads(urlopen(isscfg["jwks_uri"]).read())

            for k in aux['keys']:
                logging.notice(f"received JWT signing key {k['kid']} from issuer {iss}")
                self.__signing_keys[iss, k['kid']] = jwt.PyJWK.from_dict(k)

    def __get_signing_key_from_jwt(self, tok):
        rawtoken = jwt.api_jwt.decode_complete(tok, options={"verify_signature": False})
        kid = rawtoken["header"]["kid"]
        iss = rawtoken["payload"]["iss"]

        try:
            return self.__signing_keys[iss, kid]

        except KeyError:
            raise Exception("No matching keys")

    @staticmethod
    def getChallenge(request):
        """
        Generate the challenge for use in the WWW-Authenticate header

        @param request: The L{twisted.web.http.Request}

        @return: The C{dict} that can be used to generate a WWW-Authenticate
            header.
        """
        if hasattr(request, "_jwt_error"):
            return {"error": "invalid_token", "error_description": request._jwt_error}

        return {}

    def decode(self, response, request):
        """Returns JSONWebToken (Credentials)

        Create a JSONWebToken object from the given authentication_str.

        response    str. The bearer str minus the scheme
        request     obj. The request being processed
        """
        try:
            signing_key = self.__get_signing_key_from_jwt(response)
            payload = jwt.decode(
                response,
                signing_key.key,
                audience=self.__audience,
                algorithms=self.__algorithms,
            )

        except Exception as e:
            request._jwt_error = str(e)
            raise LoginFailed(request._jwt_error)

        if not payload:
            request._jwt_error = "No matching keys"
            raise LoginFailed(request._jwt_error)

        # compatibility with legacy EIDA token
        if "email" in payload and not "mail" in payload:
            payload["mail"] = payload["email"]

        payload["blacklisted"] = False

        return JSONWebToken(token=response, payload=payload)


@implementer(ICredentialsChecker)
class _CredentialsChecker(object):

    credentialInterfaces = [IBearerToken, IAnonymous]

    def requestAvatarId(self, credentials):
        """
        Portal will call this method if a BearerToken credential is provided.

        Check the given credentials and return a suitable user identifier if
        they are valid.

        credentials     JSONWebToken. A IBearerToken object containing a decoded token
        """
        if IBearerToken.providedBy(credentials):
            # Avatar ID is the full token payload
            return credentials.payload

        if IAnonymous.providedBy(credentials):
            # Anonymous user
            return None

        raise NotImplementedError()


@implementer(IRealm)
class _Realm(object):
    # ---------------------------------------------------------------------------
    def __init__(self, service, args, kwargs):
        self.__service = service
        self.__args = args
        self.__kwargs = kwargs

    # ---------------------------------------------------------------------------
    def requestAvatar(self, avatarId, mind, *interfaces):
        if IResource in interfaces:
            return (
                IResource,
                self.__service(
                    *self.__args,
                    **self.__kwargs,
                    user=avatarId,
                ),
                lambda: None,
            )

        raise NotImplementedError()


class JWT(object):
    def __init__(self, issuers, audience, algorithms):
        self.__cf = _CredentialFactory(issuers, audience, algorithms)

    def getAuthSessionWrapper(self, service, *args, **kwargs):
        realm = _Realm(service, args, kwargs)
        portal = Portal(realm, [_CredentialsChecker()])
        return HTTPAuthSessionWrapper(portal, [self.__cf])
