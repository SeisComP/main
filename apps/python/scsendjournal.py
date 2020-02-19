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
import seiscomp.core, seiscomp.client, seiscomp.datamodel


class SendJournal(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setDatabaseEnabled(False, False)
        self.setMessagingEnabled(True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup("EVENT")

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False
        self.params = self.commandline().unrecognizedOptions()
        if len(self.params) < 2:
            sys.stderr.write(
                self.name() + " [opts] {objectID} {action} [parameters]\n")
            return False
        return True

    def run(self):
        msg = seiscomp.datamodel.NotifierMessage()

        entry = seiscomp.datamodel.JournalEntry()
        entry.setCreated(seiscomp.core.Time.GMT())
        entry.setObjectID(self.params[0])
        entry.setSender(self.author())
        entry.setAction(self.params[1])

        sys.stderr.write(
            "Sending entry (" + entry.objectID() + "," + entry.action() + ")\n")

        if len(self.params) > 2:
            entry.setParameters(self.params[2])

        n = seiscomp.datamodel.Notifier(
            seiscomp.datamodel.Journaling.ClassName(), seiscomp.datamodel.OP_ADD, entry)
        msg.attach(n)
        self.connection().send(msg)

        return True


def main(argc, argv):
    app = SendJournal(argc, argv)
    return app()


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
