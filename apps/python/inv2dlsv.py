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

from __future__ import (absolute_import, division, print_function, unicode_literals)

import sys
import io
from seiscomp.legacy.fseed import *
from seiscomp.legacy.db.seiscomp3 import sc3wrap
from seiscomp.legacy.db.seiscomp3.inventory import Inventory
import seiscomp.datamodel, seiscomp.io

ORGANIZATION = "EIDA"


def iterinv(obj):
    return (j for i in obj.values() for j in i.values())


def main():
    if len(sys.argv) < 1 or len(sys.argv) > 3:
        sys.stderr.write("Usage inv2dlsv [in_xml [out_dataless]]\n")
        return 1

    if len(sys.argv) > 1:
        inFile = sys.argv[1]
    else:
        inFile = "-"

    if len(sys.argv) > 2:
        out = sys.argv[2]
    else:
        out = ""

    sc3wrap.dbQuery = None

    ar = seiscomp.io.XMLArchive()
    if ar.open(inFile) == False:
        raise IOError(inFile + ": unable to open")

    obj = ar.readObject()
    if obj is None:
        raise TypeError(inFile + ": invalid format")

    sc3inv = seiscomp.datamodel.Inventory.Cast(obj)
    if sc3inv is None:
        raise TypeError(inFile + ": invalid format")

    inv = Inventory(sc3inv)
    inv.load_stations("*", "*", "*", "*")
    inv.load_instruments()

    vol = SEEDVolume(inv, ORGANIZATION, "", resp_dict=False)

    for net in iterinv(inv.network):
        for sta in iterinv(net.station):
            for loc in iterinv(sta.sensorLocation):
                for strm in iterinv(loc.stream):
                    try:
                        vol.add_chan(net.code, sta.code, loc.code,
                                     strm.code, strm.start, strm.end)

                    except SEEDError as e:
                        sys.stderr.write("Error (%s,%s,%s,%s): %s\n" % (
                            net.code, sta.code, loc.code, strm.code, str(e)))

    if not out or out == "-":
        output = io.BytesIO()
        vol.output(output)
        stdout = sys.stdout.buffer if hasattr(sys.stdout, "buffer") else sys.stdout
        stdout.write(output.getvalue())
        stdout.flush()
        output.close()
    else:
        with open(sys.argv[2], "wb") as fd:
            vol.output(fd)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        sys.stderr.write("Error: %s" % str(e))
        sys.exit(1)
