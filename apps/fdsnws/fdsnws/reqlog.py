import os
import datetime
import json
import hashlib
import subprocess
import logging
import logging.handlers
import threading


from .utils import b_str

mutex = threading.Lock()


class MyFileHandler(logging.handlers.TimedRotatingFileHandler):
    def __init__(self, filename):
        super().__init__(filename, when="midnight", utc=True)

    def rotate(self, source, dest):
        super().rotate(source, dest)

        if os.path.exists(dest):
            subprocess.Popen(["bzip2", dest])


class Tracker:
    def __init__(self, logger, geoip, service, userName, userIP, clientID, userSalt):
        self.__logger = logger
        self.__userName = userName
        self.__userSalt = userSalt
        self.__logged = False

        if userName:
            userID = int(
                hashlib.md5(b_str(userSalt + userName.lower())).hexdigest()[:8], 16
            )
        else:
            userID = int(hashlib.md5(b_str(userSalt + userIP)).hexdigest()[:8], 16)

        self.__data = {
            "service": service,
            "userID": userID,
            "clientID": clientID,
            "userEmail": None,
            "auth": bool(userName),
            "userLocation": {},
            "created": f"{datetime.datetime.utcnow().isoformat()}Z",
        }

        if geoip:
            self.__data["userLocation"]["country"] = geoip.country_code_by_addr(userIP)

        if (
            (userName and userName.lower().endswith("@gfz-potsdam.de"))
            or (userName and userName.lower().endswith("@gfz.de"))
            or userIP.startswith("139.17.")
        ):
            self.__data["userLocation"]["institution"] = "GFZ"

    # pylint: disable=W0613
    def line_status(
        self,
        start_time,
        end_time,
        network,
        station,
        channel,
        location,
        restricted,
        net_class,
        shared,
        constraints,
        volume,
        status,
        size,
        message,
    ):
        try:
            trace = self.__data["trace"]

        except KeyError:
            trace = []
            self.__data["trace"] = trace

        trace.append(
            {
                "net": network,
                "sta": station,
                "loc": location,
                "cha": channel,
                "start": start_time.iso(),
                "end": end_time.iso(),
                "restricted": restricted,
                "status": status,
                "bytes": size,
            }
        )

        if restricted and status == "OK":
            self.__data["userEmail"] = self.__userName

    # FDSNWS requests have one volume, so volume_status() is called once per request
    def volume_status(self, volume, status, size, message):
        self.__data["status"] = status
        self.__data["bytes"] = size
        self.__data["finished"] = f"{datetime.datetime.utcnow().isoformat()}Z"

    def request_status(self, status, message):
        with mutex:
            if not self.__logged:
                self.__logger.info(json.dumps(self.__data))
                self.__logged = True


class RequestLog:
    def __init__(self, filename, userSalt):
        self.__logger = logging.getLogger("seiscomp.fdsnws.reqlog")
        self.__logger.addHandler(MyFileHandler(filename))
        self.__logger.setLevel(logging.INFO)
        self.__userSalt = userSalt

        try:
            import GeoIP

            self.__geoip = GeoIP.new(GeoIP.GEOIP_MEMORY_CACHE)

        except ImportError:
            self.__geoip = None

    def tracker(self, service, userName, userIP, clientID):
        return Tracker(
            self.__logger,
            self.__geoip,
            service,
            userName,
            userIP,
            clientID,
            self.__userSalt,
        )
