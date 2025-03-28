#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) GFZ Potsdam                                                #
# All rights reserved.                                                     #
#                                                                          #
# GNU Affero General Public License Usage                                  #
# This file may be used under the terms of the GNU Affero                  #
# Public License version 3.0 as published by the Free Software Foundation  #
# and appearing in the file LICENSE included in the packaging of this      #
# file. Please review the following information to ensure the GNU Affero   #
# Public License version 3.0 requirements will be met:                     #
# https://www.gnu.org/licenses/agpl-3.0.html.                              #
############################################################################

import sys
import os
import subprocess
import glob
import seiscomp.client


class Importer(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(False, False)

        self._args = argv[1:]

    def run(self):
        if len(self._args) == 0:
            sys.stderr.write("Usage: import_inv [{format}|help] <input> [output]\n")
            return False

        if self._args[0] == "help":
            if len(self._args) < 2:
                sys.stderr.write("'help' can only be used with 'formats'\n")
                sys.stderr.write("import_inv help formats\n")
                return False

            if self._args[1] == "formats":
                return self.printFormats()

            sys.stderr.write(f"unknown topic '{self._args[1]}'\n")
            return False

        fmt = self._args[0]
        try:
            prog = os.path.join(os.environ["SEISCOMP_ROOT"], "bin", f"{fmt}2inv")
        except:
            sys.stderr.write(
                "Could not get SeisComP root path, SEISCOMP_ROOT not set?\n"
            )
            return False

        if not os.path.exists(prog):
            sys.stderr.write(f"Format '{fmt}' is not supported\n")
            return False

        if len(self._args) < 2:
            sys.stderr.write("Input missing\n")
            return False

        input = self._args[1]

        if len(self._args) < 3:
            filename = os.path.basename(os.path.abspath(input))
            if not filename:
                filename = fmt

            # Append .xml if the ending is not already .xml
            if filename[-4:] != ".xml":
                filename = filename + ".xml"
            storage_dir = os.path.join(os.environ["SEISCOMP_ROOT"], "etc", "inventory")
            output = os.path.join(storage_dir, filename)
            try:
                os.makedirs(storage_dir)
            except:
                pass
            sys.stderr.write(f"Generating output to {output}\n")
        else:
            output = self._args[2]

        proc = subprocess.Popen(
            [prog, input, output], stdout=None, stderr=None, shell=False
        )
        chans = proc.communicate()
        if proc.returncode != 0:
            sys.stderr.write("Conversion failed, return code: %d\n" % proc.returncode)
            return False

        return True

    def printFormats(self):
        try:
            path = os.path.join(os.environ["SEISCOMP_ROOT"], "bin", "*2inv")
        except:
            sys.stderr.write(
                "Could not get SeisComP root path, SEISCOMP_ROOT not set?\n"
            )
            return False

        files = glob.glob(path)
        formats = []
        for f in files:
            prog = os.path.basename(f)
            formats.append(prog[: prog.find("2inv")])

        formats.sort()
        sys.stdout.write("%s\n" % "\n".join(formats))

        return True

    def printUsage(self):

        print(
            """Usage:
  import_inv [FORMAT] input [output]
  import_inv help [topic]

Import inventory information from various sources."""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            """Examples:
List all supported inventory formats
  import_inv help formats

Convert from FDSN stationXML to SeisComp format
  import_inv fdsnxml inventory_fdsnws.xml inventory_sc.xml
"""
        )


if __name__ == "__main__":
    app = Importer(len(sys.argv), sys.argv)
    sys.exit(app())
