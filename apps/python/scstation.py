#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) 2026 by Mustafa Comoglu                                    #
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
#                                                                          #
# Enable or disable stations in the config module. The state is the same  #
# as shown in the "Enabled" and "Disabled" tabs of scrttv: the             #
# ConfigStation.enabled flag is toggled and the change is sent as          #
# notifiers to the CONFIG group, so running applications update           #
# immediately and scmaster stores it in the database.                      #
############################################################################

import fnmatch
import sys

import seiscomp.client
import seiscomp.core
import seiscomp.datamodel


class StationState(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setLoadConfigModuleEnabled(True)
        self.setPrimaryMessagingGroup("CONFIG")
        self.setMessagingUsername("")
        self.setAutoApplyNotifierEnabled(False)
        self.setInterpretNotifierEnabled(False)

        self._command = None
        self._patterns = []
        self._group = "CONFIG"
        self._dryRun = False
        self._filter = None  # for "list": None, True (enabled), False (disabled)

    # ------------------------------------------------------------------ CLI
    def createCommandLineDescription(self):
        cl = self.commandline()
        cl.addGroup("Stations")
        cl.addStringOption(
            "Stations",
            "input,i",
            "Read station IDs (NET.STA, one per line, # comments) from file. "
            "Use '-' for stdin.",
        )
        cl.addStringOption(
            "Stations",
            "group",
            "Messaging group to send the change to (default: CONFIG).",
        )
        cl.addOption(
            "Stations", "dry-run", "Show what would change without sending."
        )

    def printUsage(self):
        print(
            """Usage:
  scstation [options] disable NET.STA [NET.STA ...]
  scstation [options] enable  NET.STA [NET.STA ...]
  scstation [options] list    [enabled|disabled] [NET.STA ...]

Enable or disable stations in the config module, the same state that
scrttv shows in its "Enabled" and "Disabled" tabs.

Station IDs may contain shell-style wildcards (* ? [..]), e.g. 'AU.*'.
Wildcards only match stations that already have a station configuration
in the config module; plain IDs without one get one created, as scrttv
does."""
        )
        seiscomp.client.Application.printUsage(self)
        print(
            """Examples:
  scstation -H localhost disable AU.MANU AU.WRKA
  scstation -H localhost enable 'AU.*'
  scstation -H localhost list disabled
  scstation -d mysql://sysop:sysop@localhost/seiscomp list
"""
        )

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        args = list(self.commandline().unrecognizedOptions())
        if not args:
            self.printUsage()
            return False

        self._command = args.pop(0).lower()
        if self._command not in ("enable", "disable", "list"):
            print(f"unknown command: {self._command}", file=sys.stderr)
            return False

        if self._command == "list" and args and args[0].lower() in (
            "enabled",
            "disabled",
        ):
            self._filter = args.pop(0).lower() == "enabled"

        try:
            fn = self.commandline().optionString("input")
            self._patterns.extend(self._readFile(fn))
        except RuntimeError:
            pass
        except OSError as e:
            print(f"cannot read station list: {e}", file=sys.stderr)
            return False

        for a in args:
            # allow comma separated lists as well
            self._patterns.extend(p for p in a.split(",") if p)

        for p in self._patterns:
            if p.count(".") != 1:
                print(f"invalid station ID '{p}', expected NET.STA", file=sys.stderr)
                return False

        if self._command in ("enable", "disable") and not self._patterns:
            print(f"{self._command}: no stations given", file=sys.stderr)
            return False

        try:
            self._group = self.commandline().optionString("group")
        except RuntimeError:
            pass

        self._dryRun = self.commandline().hasOption("dry-run")

        # Read-only runs with an explicit database (-d) do not need messaging
        if (self._command == "list" or self._dryRun) and self.databaseURI():
            self.setMessagingEnabled(False)

        return True

    @staticmethod
    def _readFile(fn):
        fh = sys.stdin if fn == "-" else open(fn, "r", encoding="utf-8")
        try:
            out = []
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if line:
                    out.extend(p for p in line.replace(",", " ").split() if p)
            return out
        finally:
            if fh is not sys.stdin:
                fh.close()

    # ------------------------------------------------------------- helpers
    def _configStations(self, module):
        return [module.configStation(i) for i in range(module.configStationCount())]

    def _match(self, cs, pattern):
        net, sta = pattern.split(".")
        return fnmatch.fnmatchcase(cs.networkCode(), net) and fnmatch.fnmatchcase(
            cs.stationCode(), sta
        )

    @staticmethod
    def _isWildcard(s):
        return any(c in s for c in "*?[")

    def _touch(self, cs):
        """Update creation info like scrttv does."""
        now = seiscomp.core.Time.GMT()
        try:
            ci = cs.creationInfo()
            ci.setModificationTime(now)
        except ValueError:
            ci = seiscomp.datamodel.CreationInfo()
            ci.setCreationTime(now)
        ci.setAuthor(self.author())
        ci.setAgencyID(self.agencyID())
        cs.setCreationInfo(ci)

    # ----------------------------------------------------------------- run
    def run(self):
        module = self.configModule()
        if module is None:
            print(
                f"config module '{self.configModuleName()}' not found in database "
                "(run 'seiscomp update-config' first)",
                file=sys.stderr,
            )
            return False

        if self._command == "list":
            return self._list(module)
        return self._setState(module, self._command == "enable")

    def _list(self, module):
        rows = []
        for cs in self._configStations(module):
            if self._patterns and not any(self._match(cs, p) for p in self._patterns):
                continue
            if self._filter is not None and cs.enabled() != self._filter:
                continue
            rows.append((cs.networkCode(), cs.stationCode(), cs.enabled()))

        for net, sta, en in sorted(rows):
            print(f"{net}.{sta:<8} {'enabled' if en else 'disabled'}")

        print(
            f"# {len(rows)} station(s), config module '{module.name()}'",
            file=sys.stderr,
        )
        return True

    def _setState(self, module, enable):
        state = "enabled" if enable else "disabled"
        existing = self._configStations(module)

        targets = {}  # (net, sta) -> ConfigStation or None (to be created)
        for p in self._patterns:
            matched = [cs for cs in existing if self._match(cs, p)]
            if matched:
                for cs in matched:
                    targets[(cs.networkCode(), cs.stationCode())] = cs
            elif self._isWildcard(p):
                print(f"warning: '{p}' matches no configured station", file=sys.stderr)
            else:
                targets.setdefault(tuple(p.split(".")), None)

        changed = 0
        seiscomp.datamodel.Notifier.Enable()
        try:
            for (net, sta), cs in sorted(targets.items()):
                if cs is None:
                    print(f"{net}.{sta}: no station config, creating it -> {state}")
                    if self._dryRun:
                        changed += 1
                        continue
                    cs = seiscomp.datamodel.ConfigStation.Create(
                        f"Config/{module.name()}/{net}/{sta}"
                    )
                    cs.setNetworkCode(net)
                    cs.setStationCode(sta)
                    cs.setEnabled(enable)
                    self._touch(cs)
                    module.add(cs)
                    changed += 1
                    continue

                if cs.enabled() == enable:
                    print(f"{net}.{sta}: already {state}")
                    continue

                print(f"{net}.{sta}: {'disabled' if enable else 'enabled'} -> {state}")
                if self._dryRun:
                    changed += 1
                    continue
                cs.setEnabled(enable)
                self._touch(cs)
                cs.update()
                changed += 1
        finally:
            seiscomp.datamodel.Notifier.Disable()

        msg = seiscomp.datamodel.Notifier.GetMessage(True)

        if self._dryRun:
            seiscomp.datamodel.Notifier.Clear()
            print(f"# dry run: {changed} station(s) would change")
            return True

        if changed == 0 or msg is None:
            print("# nothing to do")
            return True

        if not self.connection():
            print("not connected to messaging, cannot send change", file=sys.stderr)
            return False

        if not self.connection().send(self._group, msg):
            print(f"failed to send message to group {self._group}", file=sys.stderr)
            return False

        print(
            f"# {changed} station(s) set {state}, sent to {self._group}",
        )
        return True


def main(argc, argv):
    app = StationState(argc, argv)
    return app()


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
