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
import subprocess
import sys

from utils import diff

TIMEOUT = 5.0


class TestOffline:

    def __init__(self):
        self.rootdir = os.environ.get("SEISCOMP_ROOT")
        # print(os.environ)

    def test(self, name, inputFile, ignoreRanges):
        inputFile = os.path.join(self.rootdir, inputFile)
        outputFile = f"scevent-{name}.stdout"
        errorFile = f"scevent-{name}.stderr"

        cmd = ["scevent", "--debug", "-f", "--ep", inputFile]

        print(f"running scevent command: {' '.join(cmd)} >{outputFile} 2>{errorFile}")
        try:

            with open(inputFile, "r", encoding="utf-8") as fdIn:
                with open(outputFile, "w", encoding="utf-8") as fdOut:
                    with open(errorFile, "w", encoding="utf-8") as fdErr:
                        subprocess.run(
                            cmd,
                            stdin=fdIn,
                            stdout=fdOut,
                            stderr=fdErr,
                            timeout=TIMEOUT,
                            check=True,
                        )

        except Exception as e:
            raise ValueError(f"invalid scevent test run: {name}") from e

        expectedOutput = os.path.join(self.rootdir, f"output/{name}.xml")
        try:
            with open(expectedOutput, "r", encoding="utf-8") as fd:
                expected = fd.read()
            with open(outputFile, "r", encoding="utf-8") as fd:
                got = fd.read()
            errPos, errMsg = diff(expected, got, ignoreRanges)
        except Exception as e:
            raise ValueError(
                f"could not compare results of scevent test run: {name}"
            ) from e

        if errPos is not None:
            raise ValueError(
                f"unexpected content in scevent test run '{name}' at byte {errPos}: "
                f"{errMsg}"
            )

    def __call__(self):
        print("Testing scevent in offline mode")

        tests = [
            (
                "offline-1",
                "input/tb1/1.xml",
                [
                    (22247, 22251, 4, 30),  # hostname in author
                    (22283, 22309, 8),  # creationTime
                ],
            ),
            (
                "offline-2",
                "input/tb1/2.xml",
                [
                    (21591, 21595, 4, 30),  # hostname in author
                    (21627, 21653, 8),  # creationTime
                ],
            ),
            (
                "offline-1_2",
                "input/tb1/1_2.xml",
                [
                    (43492, 43498, 4, 30),  # hostname in author
                    (43530, 43556, 8),  # creationTime
                    (44006, 44010, 4, 30),  # hostname in author
                    (44042, 44068, 8),  # creationTime
                ],
            ),
            (
                "offline-2_1",
                "input/tb1/2_1.xml",
                [
                    (43495, 43499, 4, 30),  # hostname in author
                    (43531, 43557, 8),  # creationTime
                    (44014, 44018, 4, 30),  # hostname in author
                    (44050, 44076, 8),
                ],  # creationTime
            ),
        ]
        for name, inputFile, ignoreRanges in tests:
            self.test(name, inputFile, ignoreRanges)


# ------------------------------------------------------------------------------
if __name__ == "__main__":
    app = TestOffline()
    sys.exit(app())
