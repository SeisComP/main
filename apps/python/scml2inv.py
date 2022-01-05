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
import getopt
import seiscomp.io
import seiscomp.datamodel


usage = """scml2inv [options] input output=stdout

Options:
  -h [ --help ]  Produce help message
  -f             Enable formatted XML output
"""


def main(argv):
    formatted = False

    # parse command line options
    try:
        opts, args = getopt.getopt(argv[1:], "hf", ["help"])
    except getopt.error as msg:
        sys.stderr.write("%s\n" % msg)
        sys.stderr.write("for help use --help\n")
        return 1

    for o, a in opts:
        if o in ["-h", "--help"]:
            sys.stderr.write("%s\n" % usage)
            return 1
        elif o in ["-f"]:
            formatted = True

    argv = args
    if len(argv) < 1:
        sys.stderr.write("Missing input file\n")
        return 1

    ar = seiscomp.io.XMLArchive()
    if not ar.open(argv[0]):
        sys.stderr.write("Unable to parse input file: %s\n" % argv[0])
        return 2

    obj = ar.readObject()
    ar.close()

    if obj is None:
        sys.stderr.write("Empty document in %s\n" % argv[0])
        return 3

    inv = seiscomp.datamodel.Inventory.Cast(obj)
    if inv is None:
        sys.stderr.write("No inventory found in %s\n" % argv[0])
        return 4

    if len(argv) < 2:
        output_file = "-"
    else:
        output_file = argv[1]

    ar.create(output_file)
    ar.setFormattedOutput(formatted)
    ar.writeObject(inv)
    ar.close()

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
