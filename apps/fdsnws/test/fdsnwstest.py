#!/usr/bin/env python3

###############################################################################
# Copyright (C) 2013-2014 by gempa GmbH
#
# Author:  Stephan Herrnkind
# Email:   herrnkind@gempa.de
###############################################################################

import os
import socket
import subprocess
import sys
import time
import traceback

from threading import Thread
from datetime import datetime, timedelta

from queue import Queue

import requests

from seiscomp.fdsnws.utils import b_str  # pylint: disable=C0413


###############################################################################
class FDSNWSTest:
    # --------------------------------------------------------------------------
    def __init__(self, port=9980):
        self.port = port
        self.url = f"http://localhost:{self.port}/fdsnws"
        self.service = None
        self.rootdir = os.environ.get("SEISCOMP_ROOT")
        self.sharedir = f"{self.rootdir}/share/fdsnws"
        self.extraArgs = []

    # --------------------------------------------------------------------------
    def __call__(self):
        if not self._startService():
            return 1

        try:
            self.test()
        except Exception:
            traceback.print_exc()

            self._stopService()
            return 1

        self._stopService()
        return 0

    # --------------------------------------------------------------------------
    def _waitForSocket(self, timeout=10):
        print(f"waiting for port {self.port} to become ready ", end="")
        maxTime = datetime.now() + timedelta(timeout)
        while self.service is not None and self.service.poll() is None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            res = sock.connect_ex(("127.0.0.1", self.port))
            sock.close()
            if res == 0:
                print(" OK")
                return True

            if datetime.now() > maxTime:
                print(" TIMEOUT EXCEEDED")
                return False

            time.sleep(0.2)
            print(".", end="")

        print(" SERVICE TERMINATED")
        return False

    # --------------------------------------------------------------------------
    def _startService(self):
        cmd = self.command()
        print("starting FDSNWS service:", " ".join(cmd))
        try:
            with open("fdsnws.stdout", "w", encoding="utf-8") as fdOut:
                with open("fdsnws.stderr", "w", encoding="utf-8") as fdErr:
                    self.service = subprocess.Popen(cmd, stdout=fdOut, stderr=fdErr)
        except Exception as e:
            print(f"failed to start FDSNWS service: {e}")
            return False

        if not self._waitForSocket():
            self._stopService()
            return False

        return True

    # --------------------------------------------------------------------------
    def _stopService(self, timeout=10):
        if self.service.poll() is not None:
            print("warning: FDSNWS service terminated ahead of time", file=sys.stdout)
            return

        print(f"stopping FDSNWS service (PID: {self.service.pid}): ", end="")
        maxTime = datetime.now() + timedelta(timeout)

        self.service.terminate()
        while self.service.poll() is None:
            print(".", end="")
            time.sleep(0.2)
            if datetime.now() > maxTime:
                print(" TIMEOUT EXCEEDED, sending kill signal", file=sys.stdout)
                self.service.kill()
                return

        print(" OK")

    # --------------------------------------------------------------------------
    def test(self):
        pass

    # --------------------------------------------------------------------------
    def command(self):
        return [
            sys.executable,
            f"{self.rootdir}/../../fdsnws.py",
            "--debug",
            "--plugins=dbsqlite3,fdsnxml",
            f"--database=sqlite3://{self.rootdir}/seiscomp.sqlite3",
            "--serveAvailability=true",
            "--dataAvailability.enable=true",
            "--agencyID=Test",
            f"--record-url=sdsarchive://{self.rootdir}/sds",
            f"--htpasswd={self.rootdir}/fdsnws.htpasswd",
            f"--stationFilter={self.rootdir}/stationFilter.cfg",
            f"--port={self.port}",
        ] + self.extraArgs

    # --------------------------------------------------------------------------
    @staticmethod
    def diff(expected, got, ignoreRanges):
        if expected == got:
            return (None, None)

        lenExp = minLen = maxLen = len(expected)
        lenGot = len(got)
        if ignoreRanges:
            for r in ignoreRanges:
                if len(r) > 2:
                    minLen -= r[2]
                if len(r) > 3:
                    maxLen += r[3]

        if lenGot == 0 and minLen <= 0:
            return (None, None)

        if lenGot < minLen or lenGot > maxLen:
            exp = minLen if minLen == maxLen else f"{minLen}-{maxLen}"
            return (min(lenExp, lenGot), f"read {lenGot} bytes, expected {exp}")

        # offset between got and expected index may result from variable length
        # result data, e.g. microseconds of time stamps
        iGot = iExp = 0
        while iExp < lenExp:
            if iGot >= lenGot:
                iGot = lenGot + 1
                break

            if got[iGot] == expected[iExp]:
                iExp += 1
                iGot += 1
                continue

            # bytes do not match, check ignore Range
            ignoreRange = None
            if ignoreRanges:
                for r in ignoreRanges:
                    if r[0] <= iExp < r[1]:
                        ignoreRange = r
                        break

            if ignoreRange:
                rEnd = ignoreRange[1]
                rLeft = rEnd - iExp
                rFewer = ignoreRange[2] if len(ignoreRange) > 2 else 0
                rMore = ignoreRange[3] if len(ignoreRange) > 3 else 0
                varLen = rFewer + rMore

                # advance expected pointer behind range
                iExp = rEnd
                exp = expected[iExp] if iExp < lenExp else None

                # static range length: advance got pointer behind range
                if varLen == 0:
                    iGot += rLeft
                    continue

                # dynamic ignore range length: search end of range indicated
                # by current exp pointer but limited by rLenDiff
                iGot += min(rLeft, rLeft - rFewer)

                # expected data ends on ignore range
                if exp is None:
                    iGot += min(lenGot - iGot, varLen)
                    continue

                # search range end in data
                pos = got[iGot : iGot + varLen + 1].find(exp)
                if pos >= 0:
                    iGot += pos
                    continue

            return (
                iGot,
                f"... [ {got[max(0, iGot - 10) : min(lenGot, iGot + 11)]} ] "
                f"!= [ {expected[max(0, iExp - 10) : min(lenExp, iExp + 11)]} ] ...",
            )

        if iGot < lenGot:
            return (lenGot, f"read {lenGot - iGot} more bytes than expected")

        if iGot > lenGot:
            return (lenGot, f"read {iGot - lenGot} fewer bytes than expected")

        # should not happen
        return (None, None)

    # --------------------------------------------------------------------------
    def testHTTP(
        self,
        url,
        contentType="text/html",
        ignoreRanges=None,
        concurrent=False,
        retCode=200,
        testID=None,
        auth=False,
        data=None,
        dataFile=None,
        diffContent=True,
        silent=False,
        postData=None,
        reqHeaders=None,
        respHeaders=None,
    ):
        # pylint: disable=R0913
        if concurrent:
            self.testHTTPConcurrent(
                url,
                contentType,
                data,
                dataFile,
                retCode,
                testID,
                ignoreRanges,
                auth,
                diffContent,
                postData,
                reqHeaders=reqHeaders,
                respHeaders=respHeaders,
            )
        else:
            self.testHTTPOneShot(
                url,
                contentType,
                data,
                dataFile,
                retCode,
                testID,
                ignoreRanges,
                auth,
                diffContent,
                silent,
                postData,
                reqHeaders=reqHeaders,
                respHeaders=respHeaders,
            )

    # --------------------------------------------------------------------------
    def testHTTPOneShot(
        self,
        url,
        contentType="text/html",
        data=None,
        dataFile=None,
        retCode=200,
        testID=None,
        ignoreRanges=None,
        auth=False,
        diffContent=True,
        silent=False,
        postData=None,
        reqHeaders=None,
        respHeaders=None,
    ):
        # pylint: disable=R0913
        if not silent:
            if testID is not None:
                print(f"#{testID} ", end="")
            print(f"{url}: ", end="")
        stream = dataFile is not None
        dAuth = requests.auth.HTTPDigestAuth("sysop", "sysop") if auth else None
        if postData is None:
            r = requests.get(
                url, stream=stream, auth=dAuth, headers=reqHeaders, timeout=10
            )
        else:
            r = requests.post(
                url,
                data=postData,
                stream=stream,
                auth=dAuth,
                headers=reqHeaders,
                timeout=10,
            )

        if r.status_code != retCode:
            raise ValueError(
                f"Invalid status code, expected {retCode}, got {r.status_code}"
            )

        if contentType is not None and contentType != r.headers["content-type"]:
            raise ValueError(
                f"Invalid content type, expected {contentType}, got "
                f"{r.headers['content-type']}"
            )

        if respHeaders is not None:
            # validate response headers
            for k, v in respHeaders.items():
                if k not in r.headers:
                    raise ValueError(f"Missing response header field: {k!r}")

                if callable(v) and v(r.headers[k]):
                    continue

                if v != r.headers[k]:
                    raise ValueError(
                        "Invalid response header field value: "
                        f"{v!r} != {r.headers[k]!r}"
                    )

        expected = None
        if data is not None:
            expected = b_str(data)
        elif dataFile is not None:
            with open(dataFile, "rb") as f:
                expected = f.read()

        if expected is not None:
            if diffContent:
                errPos, errMsg = self.diff(expected, r.content, ignoreRanges)
                if errPos is not None:
                    raise ValueError(f"Unexpected content at byte {errPos}: {errMsg}")
            else:
                if len(expected) != len(r.content):
                    raise ValueError(
                        f"Unexpected content length, expected {len(expected)}, "
                        f"got {len(r.content)}"
                    )

        if not silent:
            print("OK")
        sys.stdout.flush()

    # --------------------------------------------------------------------------
    def testHTTPConcurrent(
        self,
        url,
        contentType="text/html",
        data=None,
        dataFile=None,
        retCode=200,
        testID=None,
        ignoreRanges=None,
        auth=False,
        diffContent=True,
        postData=None,
        repetitions=1000,
        numThreads=10,
        reqHeaders=None,
        respHeaders=None,
    ):
        # pylint: disable=R0913
        if testID is not None:
            print(f"#{testID} ", end="")
        print(f"concurrent [{repetitions}/{numThreads}] {url}: ", end="")
        sys.stdout.flush()

        def doWork():
            while True:
                try:
                    i = q.get()
                    if i is None:
                        break
                    self.testHTTPOneShot(
                        url,
                        contentType,
                        data,
                        dataFile,
                        retCode,
                        testID,
                        ignoreRanges,
                        auth,
                        diffContent,
                        True,
                        postData,
                        reqHeaders=reqHeaders,
                        respHeaders=respHeaders,
                    )
                    print(".", end="")
                    sys.stdout.flush()
                except ValueError as e:
                    errors.append(f"error in job #{i}: {str(e)}")
                finally:
                    q.task_done()

        # queue
        q = Queue()
        errors = []

        # start worker threads
        threads = []
        for i in range(numThreads):
            t = Thread(target=doWork)
            t.start()
            threads.append(t)

        # populate queue with work
        for i in range(repetitions):
            q.put(i)
        q.join()

        # stop worker
        for i in range(numThreads):
            q.put(None)
        for t in threads:
            t.join()

        if errors:
            raise ValueError(f"{len(errors)} errors occured, first one is: {errors[0]}")

        print(" OK")
        sys.stdout.flush()


# vim: ts=4 et tw=88
