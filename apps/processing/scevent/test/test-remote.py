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

import requests
import shutil
import sys

from utils import Service, ManagedService, ManagedDispatchReceive

TIMEOUT = 5.0

PORT_SCHUB_A = 9981
PORT_SCHUB_B = 9982
PORT_SCEVENT_A = 9991
PORT_SCEVENT_B = 9992


class Schub(Service):
    def setup(self):
        shutil.copy(f"{self.rootdir}/seiscomp.db", self.dbPath())

    def dbPath(self):
        return f"{self.name}.db"

    def command(self):
        db = self.dbPath()
        return [
            "scmaster",
            "--logging.level=4",
            f"--log-file={self.name}.log",
            "--core.plugins=dbsqlite3",
            f"--interface.bind=127.0.0.1:{self.port}",
            "--queues=production",
            "--queues.production.plugins=dbstore",
            "--queues.production.processors.messages=dbstore",
            "--queues.production.processors.messages.dbstore.driver=sqlite3",
            f"--queues.production.processors.messages.dbstore.read={db}",
            f"--queues.production.processors.messages.dbstore.write={db}",
            "--queues.production.groups=LOCATION,MAGNITUDE,FOCMECH,EVENT",
        ]


class SCEvent(Service):
    def __init__(
        self, name, restPort, schubPort, idPrefix, leaderPort=None, cacheDB=None
    ):
        super().__init__(name, restPort)
        self.schubPort = schubPort
        self.idPrefix = idPrefix
        self.leaderPort = leaderPort
        self.cacheDB = cacheDB

    def command(self):
        cmd = [
            "scevent",
            "--logging.level=4",
            f"--log-file={self.name}.log",
            "--plugins=dbsqlite3",
            "-H",
            f"127.0.0.1:{self.schubPort}",
            f"--eventIDPrefix={self.idPrefix}",
            f"--restAPI=127.0.0.1:{self.port}",
        ]
        if self.leaderPort:
            cmd.append(f"--eventIDSync.main=http://127.0.0.1:{self.leaderPort}")
        if self.cacheDB:
            cmd.append(f"--eventIDSync.db={self.cacheDB}")

        return cmd


def assertResult(context, expected, got):
    if expected != got:
        raise ValueError(f"invalid {context}, expected {expected}, got {got}")


class TestRemote:

    def __init__(self):
        self.rootdir = os.environ.get("SEISCOMP_ROOT")

    def testAPI(self, tb1File1, tb1File2, tb2File1, tb2File2):
        with ManagedService(SCEvent("sceventA", PORT_SCEVENT_A, PORT_SCHUB_A, "A")):
            with open(tb1File1, "r", encoding="utf-8") as fd:
                tb1Data1 = fd.read()
            with open(tb1File2, "r", encoding="utf-8") as fd:
                tb1Data2 = fd.read()
            with open(tb2File1, "r", encoding="utf-8") as fd:
                tb2Data1 = fd.read()
            with open(tb2File2, "r", encoding="utf-8") as fd:
                tb2Data2 = fd.read()

            URL = f"http://127.0.0.1:{PORT_SCEVENT_A}/api/1/try-to-associate"
            URL_ALLOC = f"{URL}?allocate"
            HEADERS = {"content-type": "text/xml"}

            eventID1 = "A2026kmhs"
            eventID2 = "A2026kmht"

            # no alloc and not events so far
            r = requests.post(URL, data=tb1Data1, headers=HEADERS, timeout=TIMEOUT)
            assertResult("status code", 204, r.status_code)
            r = requests.post(URL, data=tb1Data2, headers=HEADERS, timeout=TIMEOUT)
            assertResult("status code", 204, r.status_code)

            # alloc eventID
            r = requests.post(
                URL_ALLOC, data=tb1Data1, headers=HEADERS, timeout=TIMEOUT
            )
            assertResult("status code", 200, r.status_code)
            assertResult("content", eventID1, r.text)

            # same origin, without alloc -> no eventID
            r = requests.post(URL, data=tb1Data1, headers=HEADERS, timeout=TIMEOUT)
            assertResult("status code", 204, r.status_code)

            # same origin with alloc -> same eventID
            r = requests.post(
                URL_ALLOC, data=tb1Data1, headers=HEADERS, timeout=TIMEOUT
            )
            assertResult("status code", 200, r.status_code)
            assertResult("content", eventID1, r.text)

            # simmilar origin -> same eventID
            r = requests.post(
                URL_ALLOC, data=tb2Data1, headers=HEADERS, timeout=TIMEOUT
            )
            assertResult("status code", 200, r.status_code)
            assertResult("content", eventID1, r.text)

            # 2nd origin with different location but same time -> new eventID
            r = requests.post(
                URL_ALLOC, data=tb2Data2, headers=HEADERS, timeout=TIMEOUT
            )
            assertResult("status code", 200, r.status_code)
            assertResult("content", eventID2, r.text)

            # simmilar 2nd origin -> same eventID
            r = requests.post(
                URL_ALLOC, data=tb1Data2, headers=HEADERS, timeout=TIMEOUT
            )
            assertResult("status code", 200, r.status_code)
            assertResult("content", eventID2, r.text)

    def testEvenIDSync(self, tb1File1, tb1File2, tb2File1, tb2File2, tb2File2a,
                       tb2File2b):
        # sceventA is the main instance; sceventB is the secondary and is
        # pointed at sceventA's REST API via leaderPort. tb1 and tb2 carry the
        # same physical earthquakes under different publicID namespaces, so a
        # working main/secondary handshake must make both instances converge on
        # the same event ID.
        with ManagedService(
            SCEvent("sceventA", PORT_SCEVENT_A, PORT_SCHUB_A, "A")
        ) as sceventA:
            with ManagedService(
                SCEvent(
                    "sceventB",
                    PORT_SCEVENT_B,
                    PORT_SCHUB_B,
                    "B",
                    leaderPort=PORT_SCEVENT_A,
                )
            ) as sceventB:

                # 1) Push the earthquake into the main's broker and capture the event
                #    the main forms.
                with ManagedDispatchReceive("dpA", PORT_SCHUB_A, tb1File1) as drA:
                    if drA.error:
                        raise ValueError(f"main side event 1: {drA.error}")
                    idMain1 = drA.eventID

                # 2) Push the same earthquake (other namespace) into the
                #    secondary's broker; the secondary must obtain the main's ID
                #    over REST and form an event with it.
                with ManagedDispatchReceive("dpB", PORT_SCHUB_B, tb2File1) as drB:
                    if drB.error:
                        raise ValueError(f"secondary side event 1: {drB.error}")
                    idSecondary1 = drB.eventID

                # Both instances must have converged on the same event ID. sceventB runs
                # with prefix "B" but, having queried the main, must adopt the main's
                # "A"-prefixed ID rather than minting its own — so equality here proves
                # the handshake actually happened.
                assertResult("synchronized eventID 1", idMain1, idSecondary1)

                # 3) Reverse the order for the second event to prove that event forming
                #    base on cached events also works
                with ManagedDispatchReceive("dpB", PORT_SCHUB_B, tb2File2) as drB:
                    if drB.error:
                        raise ValueError(f"secondary side event 2: {drB.error}")
                    idSecondary2 = drB.eventID

                with ManagedDispatchReceive("dpA", PORT_SCHUB_A, tb1File2) as drA:
                    if drA.error:
                        raise ValueError(f"main side event 2: {drA.error}")
                    idMain2 = drA.eventID

                assertResult("synchronized eventID 2", idMain2, idSecondary2)

                # 4) Send a second origin similar to tb2File2 which should be assigned
                #    to the same event
                with ManagedDispatchReceive("dpB", PORT_SCHUB_B, tb2File2a) as drB:
                    if drB.error:
                        raise ValueError(f"secondary side event 2a: {drB.error}")
                    idSecondary2a = drB.eventID

                assertResult(
                    "synchronized eventID 2 second origin", idSecondary2, idSecondary2a
                )

                originToSplit = "de.gempa.tb2.Origin/20260528101507.770537.2A"

                # 5) Split originToSplit out of its event on the SECONDARY. The
                #    split-off origin needs a brand-new event, so the secondary
                #    must query the main for the event ID (the EvSplitOrg sync
                #    path). A working handshake makes it adopt the main's
                #    freshly formed "A" ID instead of minting a "B" one.
                with ManagedDispatchReceive(
                    "dpBsplit",
                    PORT_SCHUB_B,
                    splitOrigin=originToSplit,
                    splitEvent=idSecondary2,
                ) as drB:
                    if drB.error:
                        raise ValueError(f"secondary side split: {drB.error}")
                    idSecondary2split = drB.eventID

                assertResult(
                    "split eventID prefix secondary", "A", idSecondary2split[:1]
                )
                if idSecondary2split in (idMain1, idMain2, idSecondary1, idSecondary2):
                    raise ValueError(
                        "received split eventID seen before, expected value other than "
                        f"{idSecondary2split}"
                    )

                # 6) Form a new event on the SECONDARY from a fresh origin
                #    (2b.xml, origin ...2B) via an EvNewEvent command. The origin
                #    and the command are sent together so scevent forms the event
                #    from the journal command rather than by ordinary
                #    association. As with the split, the new event needs a
                #    brand-new ID which the secondary must obtain from the main
                #    through the /allocate endpoint, so it again carries an
                #    "A"-prefixed, previously unseen ID rather than a local "B"
                #    one.
                with ManagedDispatchReceive(
                    "dpBnew",
                    PORT_SCHUB_B,
                    newEventFile=tb2File2b,
                ) as drB:
                    if drB.error:
                        raise ValueError(f"secondary side new event: {drB.error}")
                    idSecondary2new = drB.eventID

                assertResult(
                    "new event eventID prefix secondary", "A", idSecondary2new[:1]
                )
                if idSecondary2new in (
                    idMain1, idMain2, idSecondary1, idSecondary2, idSecondary2split
                ):
                    raise ValueError(
                        "received new event eventID seen before, expected value "
                        f"other than {idSecondary2new}"
                    )

    def testAPIDB(self, tb1File1, tb1File2, tb2File1, tb2File2):
        with open(tb1File1, "r", encoding="utf-8") as fd:
            tb1Data1 = fd.read()
        with open(tb1File2, "r", encoding="utf-8") as fd:
            tb1Data2 = fd.read()
        with open(tb2File1, "r", encoding="utf-8") as fd:
            tb2Data1 = fd.read()
        with open(tb2File2, "r", encoding="utf-8") as fd:
            tb2Data2 = fd.read()

        eventID1 = "A2026kmhs"
        eventID2 = "A2026kmht"

        dataIDList = [
            (tb1Data1, eventID1),
            (tb2Data1, eventID1),
            (tb2Data2, eventID2),
            (tb1Data2, eventID2),
        ]

        name = "sceventDB"
        cacheDB = f"{name}.sqlite3"

        if os.path.isfile(cacheDB):
            os.remove(cacheDB)

        URL = f"http://127.0.0.1:{PORT_SCEVENT_A}/api/1/try-to-associate"
        URL_ALLOC = f"{URL}?allocate"
        HEADERS = {"content-type": "text/xml"}

        # First run: create DB
        with ManagedService(
            SCEvent(name, PORT_SCEVENT_A, PORT_SCHUB_A, "A", cacheDB=cacheDB)
        ):
            for data, eventID in dataIDList:
                r = requests.post(
                    URL_ALLOC, data=data, headers=HEADERS, timeout=TIMEOUT
                )
                assertResult("status code", 200, r.status_code)
                assertResult("content", eventID, r.text)

        # Second run: reuse DB, push test origins in reverse order
        with ManagedService(
            SCEvent("sceventA", PORT_SCEVENT_A, PORT_SCHUB_A, "A", cacheDB=cacheDB)
        ):
            for data, eventID in reversed(dataIDList):
                r = requests.post(
                    URL_ALLOC, data=data, headers=HEADERS, timeout=TIMEOUT
                )
                assertResult("status code", 200, r.status_code)
                assertResult("content", eventID, r.text)

    def __call__(self):
        print("Testing scevent API")
        tb1File1 = os.path.join(self.rootdir, "input/tb1/1.xml")
        tb1File2 = os.path.join(self.rootdir, "input/tb1/2.xml")
        tb2File1 = os.path.join(self.rootdir, "input/tb2/1.xml")
        tb2File2 = os.path.join(self.rootdir, "input/tb2/2.xml")
        tb2File2a = os.path.join(self.rootdir, "input/tb2/2a.xml")
        tb2File2b = os.path.join(self.rootdir, "input/tb2/2b.xml")

        with ManagedService(Schub("schubA", PORT_SCHUB_A)):
            self.testAPI(tb1File1, tb1File2, tb2File1, tb2File2)
            self.testAPI(tb2File1, tb2File2, tb1File1, tb1File2)
            self.testAPIDB(tb1File1, tb1File2, tb2File1, tb2File2)

            with ManagedService(Schub("schubB", PORT_SCHUB_B)):
                self.testEvenIDSync(
                    tb1File1, tb1File2, tb2File1, tb2File2, tb2File2a, tb2File2b
                )


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestRemote()
    sys.exit(app())
