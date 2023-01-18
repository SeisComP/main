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
import seiscomp.core
import seiscomp.client
import seiscomp.datamodel


class SendJournal(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setDatabaseEnabled(False, False)
        self.setMessagingEnabled(True)
        self.setMessagingUsername("")
        self.setPrimaryMessagingGroup("EVENT")
        self.params = None
        self.filename = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Read parameters from given file instead of command line.",
        )

    def init(self):
        if not seiscomp.client.Application.init(self):
            return False

        return True

    def printUsage(self):

        print(
            """Usage:
  scsendjournal [options] {objectID} {action} [parameters]

Send journaling information to the messaging to manipulate SeisComP objects like events and origins."""
        )

        seiscomp.client.Application.printUsage(self)

        print(
            """Examples:
Set the type of the event with ID gempa2021abcd to 'earthquake'
  scsendjournal -H localhost gempa2021abcd EvType "earthquake"

Set the type of the event with ID gempa2021abcd and read the type from file
  scsendjournal -H localhost gempa2021abcd EvType -i input.txt
"""
        )

    def run(self):
        msg = seiscomp.datamodel.NotifierMessage()

        entry = seiscomp.datamodel.JournalEntry()
        entry.setCreated(seiscomp.core.Time.GMT())
        entry.setObjectID(self.params[0])
        entry.setSender(self.author())
        entry.setAction(self.params[1])

        print(
            f"Sending entry ({entry.objectID()},{entry.action()})",
            file=sys.stderr,
        )

        if self.filename:
            try:
                with open(self.filename, "r") as f:
                    entry.setParameters(f.read().rstrip())
            except Exception as err:
                print(f"{str(err)}", file=sys.stderr)
                return False

        elif len(self.params) > 2:
            entry.setParameters(self.params[2])

        n = seiscomp.datamodel.Notifier(
            seiscomp.datamodel.Journaling.ClassName(), seiscomp.datamodel.OP_ADD, entry
        )
        msg.attach(n)
        self.connection().send(msg)

        return True

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        try:
            self.filename = self.commandline().optionString("input")
        except RuntimeError:
            pass

        self.params = self.commandline().unrecognizedOptions()
        if len(self.params) < 2:
            print(
                f"{self.name()} [opts] {{objectID}} {{action}} [parameters]",
                file=sys.stderr,
            )
            return False

        return True


def main(argc, argv):
    app = SendJournal(argc, argv)
    return app()


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
