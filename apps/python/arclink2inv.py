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

import seiscomp.datamodel
import seiscomp.io
import getopt
import sys


usage = """arclink2inv [options] input=stdin output=stdout

Options:
  -h [ --help ]      Produce help message
  -f [ --formatted ] Enable formatted XML output
"""


def main(argv):
    imp = seiscomp.io.Importer.Create("arclink")
    if imp is None:
        sys.stderr.write("Arclink import not available\n")
        return 1

    formatted = False

    # parse command line options
    try:
        opts, args = getopt.getopt(argv[1:], "hf", ["help", "formatted"])
    except getopt.error as msg:
        sys.stderr.write("%s\n" % msg)
        sys.stderr.write("for help use --help\n")
        return 1

    for o, a in opts:
        if o in ["-h", "--help"]:
            sys.stderr.write("%s\n" % usage)
            return 1
        elif o in ["-f", "--formatted"]:
            formatted = True

    argv = args

    if len(argv) > 0:
        o = imp.read(argv[0])
    else:
        o = imp.read("-")

    inv = seiscomp.datamodel.Inventory.Cast(o)
    if inv is None:
        sys.stderr.write("No inventory found\n")
        return 1

    ar = seiscomp.io.XMLArchive()
    if len(argv) > 1:
        res = ar.create(argv[1])
    else:
        res = ar.create("-")

    if not res:
        sys.stderr.write("Failed to open output\n")
        return 1

    ar.setFormattedOutput(formatted)
    ar.writeObject(inv)
    ar.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
