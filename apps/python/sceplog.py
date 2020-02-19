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
import seiscomp.client, seiscomp.datamodel, seiscomp.io


class EventParameterLog(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(False, False)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup(
            seiscomp.client.Protocol.LISTENER_GROUP)
        self.addMessagingSubscription("EVENT")
        self.addMessagingSubscription("LOCATION")
        self.addMessagingSubscription("MAGNITUDE")
        self.addMessagingSubscription("AMPLITUDE")
        self.addMessagingSubscription("PICK")

        self.setAutoApplyNotifierEnabled(True)
        self.setInterpretNotifierEnabled(True)

        # EventParameter object
        self._eventParameters = seiscomp.datamodel.EventParameters()

    def run(self):
        if seiscomp.client.Application.run(self) == False:
            return False

        ar = seiscomp.io.XMLArchive()
        ar.setFormattedOutput(True)
        if ar.create("-") == True:
            ar.writeObject(self._eventParameters)
            ar.close()
            # Hack to avoid the "close failed in file object destructor"
            # exception
#     print ""
            sys.stdout.write("\n")

        return True


app = EventParameterLog(len(sys.argv), sys.argv)
sys.exit(app())
