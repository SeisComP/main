#!/usr/bin/env python3

###########################################################################
# Copyright (C) gempa GmbH                                                #
# All rights reserved.                                                    #
# Contact: gempa GmbH (seiscomp-dev@gempa.de)                             #
#                                                                         #
# GNU Affero General Public License Usage                                 #
# This file may be used under the terms of the GNU Affero                 #
# Public License version 3.0 as published by the Free Software Foundation #
# and appearing in the file LICENSE included in the packaging of this     #
# file. Please review the following information to ensure the GNU Affero  #
# Public License version 3.0 requirements will be met:                    #
# https://www.gnu.org/licenses/agpl-3.0.html.                             #
#                                                                         #
# Other Usage                                                             #
# Alternatively, this file may be used in accordance with the terms and   #
# conditions contained in a signed written agreement between you and      #
# gempa GmbH.                                                             #
###########################################################################

import os
import socket
import subprocess
import sys
import time

from contextlib import contextmanager

from datetime import datetime, timedelta

TIMEOUT = 5.0


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

    # offset between got and expected index may result from variable length result data,
    # e.g., microseconds of time stamps
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

            # dynamic ignore range length: search end of range indicated by
            # current exp pointer but limited by rLenDiff
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


class Service:

    def __init__(self, name, port=None):
        self.name = name
        self.port = port
        self.rootdir = os.environ.get("SEISCOMP_ROOT")
        self.timeout = 10
        self.service = None

        self.setup()

    def setup(self):
        pass

    def waitForSocket(self):
        print(f"[{self.name}] waiting for port {self.port} to become ready ", end="")
        maxTime = datetime.now() + timedelta(self.timeout)
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

    def startService(self):
        cmd = self.command()
        print(f"[{self.name}] starting service:", " ".join(cmd))
        try:
            with open(f"{self.name}.stdout", "w", encoding="utf-8") as fdOut:
                with open(f"{self.name}.stderr", "w", encoding="utf-8") as fdErr:
                    self.service = subprocess.Popen(cmd, stdout=fdOut, stderr=fdErr)
        except Exception as e:
            print(f"[{self.name}] failed to start service: {e}")
            return False

        if self.port and not self.waitForSocket():
            self.stopService()
            return False

        return True

    def stopService(self):
        if not self.service:
            return

        service = self.service
        self.service = None

        if service.poll() is not None:
            print(
                f"[{self.name}] warning: service terminated ahead of time",
                file=sys.stdout,
            )
            return

        print(f"[{self.name}] stopping service (PID: {service.pid}): ", end="")
        maxTime = datetime.now() + timedelta(self.timeout)

        service.terminate()
        while service.poll() is None:
            print(".", end="")
            time.sleep(0.2)
            if datetime.now() > maxTime:
                print(" TIMEOUT EXCEEDED, sending kill signal", file=sys.stdout)
                service.kill()
                return

        print(" OK")

    def command(self):
        raise NotImplementedError


@contextmanager
def ManagedService(service):
    service.startService()
    try:
        yield service
    finally:
        service.stopService()


class DispatchReceive(Service):
    """Runs the standalone dispatch-receive.py helper as a separate process.

    The helper connects to the broker, optionally sends one origin, waits for
    the resulting Event and prints 'EVENTID <publicID>' (or 'NOEVENT') to its
    stdout. Running it as its own process keeps the blocking SeisComP run loop
    on that process's main thread, avoiding the GIL/thread deadlock that made
    the threaded version hang.

    After the process has finished, the received event ID (if any) is
    available as the .eventID attribute; .error holds a message otherwise.
    """

    #: Absolute path to the helper, resolved next to this utils.py.
    HELPER = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "dispatch-receive.py"
    )

    def __init__(self, name, schubPort, originFile=None, eventTimeout=TIMEOUT):
        # No port to wait for: the helper is a messaging client, it does not
        # open a listening socket. Service.waitForSocket is therefore skipped
        # (port is None).
        super().__init__(name, port=None)
        self.schubPort = schubPort
        self.originFile = originFile
        self.eventTimeout = eventTimeout
        self.eventID = None
        self.error = None

    def command(self):
        cmd = [
            sys.executable,
            self.HELPER,
            "-H",
            f"127.0.0.1:{self.schubPort}",
            "--logging.level=4",
            f"--log-file={self.name}.log",
            f"--event-timeout={self.eventTimeout}",
        ]
        if self.originFile:
            cmd.append(f"--origin={self.originFile}")
        return cmd

    def runToCompletion(self):
        """Start the helper and wait for it to finish, then parse its stdout
        for the 'EVENTID <id>' / 'NOEVENT' line. Returns self so it can be
        used directly. Bounded by Service.timeout via the helper's own
        --event-timeout (the helper always exits on its own)."""
        if not self.startService():
            self.error = "failed to start dispatch-receive helper"
            return self

        # The helper exits by itself: it quits once it has the event or once
        # its --event-timeout elapses. Give it a little longer than that
        # before we force the issue.
        try:
            self.service.wait(timeout=self.eventTimeout + self.timeout)
        except subprocess.TimeoutExpired:
            self.error = "dispatch-receive helper did not terminate"
            self.stopService()
            return self

        # Process has exited; Service.stopService would try to terminate a
        # finished process, so just clear the handle after reading output.
        self._parseOutput()
        self.service = None
        return self

    def _parseOutput(self):
        try:
            with open(f"{self.name}.stdout", "r", encoding="utf-8") as fd:
                lines = fd.read().splitlines()
        except OSError as e:
            self.error = f"could not read helper output: {e}"
            return

        for line in lines:
            if line.startswith("EVENTID "):
                self.eventID = line[len("EVENTID ") :].strip()
                return
            if line.startswith("ERROR "):
                self.error = line[len("ERROR ") :].strip()
                return
            if line.strip() == "NOEVENT":
                self.error = f"no event received within {self.eventTimeout} s"
                return

        self.error = "helper produced no EVENTID/NOEVENT line"


@contextmanager
def ManagedDispatchReceive(name, schubPort, originFile=None, eventTimeout=TIMEOUT):
    """Context manager that runs the dispatch-receive helper to completion and
    yields the DispatchReceive object (with .eventID / .error populated)."""
    dr = DispatchReceive(name, schubPort, originFile, eventTimeout)
    dr.runToCompletion()
    try:
        yield dr
    finally:
        dr.stopService()
