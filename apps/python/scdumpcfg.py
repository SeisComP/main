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
import seiscomp.client
import seiscomp.datamodel
import seiscomp.config
import seiscomp.system


def readParams(sc_params):
    if sc_params.baseID():
        sc_params_base = seiscomp.datamodel.ParameterSet.Find(sc_params.baseID())
        if sc_params_base is None:
            print(
                f"Warning: {sc_params.baseID()}: base parameter set for "
                f"{sc_params.publicID()} not found",
                file=sys.stderr,
            )
            params = {}
        else:
            params = readParams(sc_params_base)
    else:
        params = {}

    for i in range(sc_params.parameterCount()):
        p = sc_params.parameter(i)
        params[p.name()] = p.value()

    return params


class DumpCfg(seiscomp.client.Application):
    def __init__(self, argc, argv):
        if argc < 2:
            print("scdumpcfg {modname} [options]", file=sys.stderr)
            raise RuntimeError

        self.appName = argv[1]
        self.config = seiscomp.config.Config()

        # Remove first parameter to replace appname with passed module name
        # argc = argc - 1
        # argv = argv[1:]

        seiscomp.client.Application.__init__(self, argc, argv)

        self.setMessagingEnabled(True)
        self.setMessagingUsername("")
        self.setDatabaseEnabled(True, True)
        self.setLoadConfigModuleEnabled(True)
        self.setDaemonEnabled(False)

        self.dumpBindings = False
        self.allowGlobal = False
        self.formatCfg = False
        self.nslc = False
        self.param = None

    def createCommandLineDescription(self):
        self.commandline().addGroup("Dump")
        self.commandline().addOption(
            "Dump", "bindings,B", "Dump bindings instead of module configuration."
        )
        self.commandline().addOption(
            "Dump",
            "allow-global,G",
            "Print global bindings if no module binding is avaible.",
        )
        self.commandline().addStringOption(
            "Dump",
            "param,P",
            "Specify the parameter name(s) to filter for. Use comma sepration for "
            "multiple parameters.",
        )
        self.commandline().addOption(
            "Dump", "cfg", "Print output in .cfg format. Does not work along with -B."
        )
        self.commandline().addOption(
            "Dump",
            "nslc",
            "Print the list of channels which have bindings of the given module. "
            "Requires to set -B. Can be used by other modules, e.g., invextr, scart, "
            "scmssort, scevtstreams.",
        )

    def validateParameters(self):
        if not seiscomp.client.Application.validateParameters(self):
            return False

        self.dumpBindings = self.commandline().hasOption("bindings")

        try:
            param = self.commandline().optionString("param")
            self.param = param.split(",")
        except RuntimeError:
            pass

        self.allowGlobal = self.commandline().hasOption("allow-global")
        self.formatCfg = self.commandline().hasOption("cfg")
        self.nslc = self.commandline().hasOption("nslc")

        if self.dumpBindings and self.databaseURI() != "":
            self.setMessagingEnabled(False)
            self.setDatabaseEnabled(True, False)

        if not self.dumpBindings:
            self.setMessagingEnabled(False)
            self.setDatabaseEnabled(False, False)
            self.setLoadConfigModuleEnabled(False)

        return True

    def initConfiguration(self):
        if self.appName in ("-h", "--help"):
            self.printUsage()
            return False

        if not seiscomp.client.Application.initConfiguration(self):
            return False

        seiscomp.system.Environment.Instance().initConfig(self.config, self.appName)

        return True

    def initSubscriptions(self):
        # Do nothing.
        return True

    def printUsage(self):
        print(
            f"""Usage:
  {os.path.basename(__file__)} [options]

Dump bindings or module configurations used by a specific module or global for \
particular stations.""",
            file=sys.stderr,
        )

        seiscomp.client.Application.printUsage(self)

        print(
            f"""Examples:
Dump scautopick bindings configuration including global for all stations
  {os.path.basename(__file__)} scautopick -d localhost -BG

Connect to messaging for the database connection and dump scautopick bindings \
configuration including global for all stations
  {os.path.basename(__file__)} scautopick -H localhost -BG

Dump scautopick module configuration including global parameters
  {os.path.basename(__file__)} scautopick --cfg

Dump global bindings configuration considerd by scmv
  {os.path.basename(__file__)} scmv -d localhost -BG

Dump the list of streams configured with scautopick bindings
  {os.path.basename(__file__)} scautopick -d localhost -B --nslc

Dump specific parameters configured with scautopick bindings
  {os.path.basename(__file__)} scautopick -B -d localhost \
-P spicker.AIC.minSNR,spicker.AIC.minCnt
""",
            file=sys.stderr,
        )

    def run(self):
        cfg = self.config
        if self.nslc:
            nslc = set()

        if not self.dumpBindings:
            symtab = cfg.symbolTable()
            names = cfg.names()
            count = 0
            for name in names:
                if self.param and name not in self.param:
                    continue

                sym = symtab.get(name)
                if self.formatCfg:
                    if sym.comment:
                        if count > 0:
                            print("")
                        print(f"{sym.comment}")
                    print(f"{cfg.escapeIdentifier(sym.name)} = {sym.content}")
                else:
                    print(f"{sym.name}")
                    print(f"  value(s) : {', '.join(sym.values)}")
                    print(f"  source   : {sym.uri}")

                count = count + 1

            if self.param and count == 0:
                print(f"{self.param}: definition not found", file=sys.stderr)
        else:
            cfg = self.configModule()
            if cfg is None:
                print("No config module read", file=sys.stderr)
                return False

            tmp = {}
            for i in range(cfg.configStationCount()):
                cfg_sta = cfg.configStation(i)
                tmp[(cfg_sta.networkCode(), cfg_sta.stationCode())] = cfg_sta

            name = self.appName
            # For backward compatibility rename global to default
            if name == "global":
                name = "default"

            for item in sorted(tmp.keys()):
                cfg_sta = tmp[item]
                sta_enabled = cfg_sta.enabled()
                cfg_setup = seiscomp.datamodel.findSetup(
                    cfg_sta, name, self.allowGlobal
                )

                if not cfg_setup is None:
                    suffix = ""
                    if sta_enabled and cfg_setup.enabled():
                        out = "+ "
                    else:
                        suffix = " ("
                        if not sta_enabled:
                            suffix += "station disabled"
                        if not cfg_setup.enabled():
                            if suffix:
                                suffix += ", "
                            suffix += "setup disabled"
                        suffix += ")"
                        out = "- "
                    out += f"{cfg_sta.networkCode()}.{cfg_sta.stationCode()}{suffix}\n"
                    params = seiscomp.datamodel.ParameterSet.Find(
                        cfg_setup.parameterSetID()
                    )
                    if params is None:
                        print(
                            f"ERROR: {cfg_setup.parameterSetID()}: ParameterSet not found",
                            file=sys.stderr,
                        )
                        return False

                    params = readParams(params)
                    if self.nslc:
                        try:
                            sensorLocation = params["detecLocid"]
                        except KeyError:
                            sensorLocation = ""
                        try:
                            detecStream = params["detecStream"]
                        except KeyError:
                            detecStream = ""

                        stream = f"{cfg_sta.networkCode()}.{cfg_sta.stationCode()}.{sensorLocation}.{detecStream}"
                        nslc.add(stream)
                    count = 0
                    for param_name in sorted(params.keys()):
                        if self.param and param_name not in self.param:
                            continue
                        out += f"  {param_name}: {params[param_name]}\n"
                        count = count + 1

                    if not self.nslc and count > 0:
                        print(out)

        if self.nslc:
            for stream in sorted(nslc):
                print(stream, file=sys.stdout)

        return True


try:
    app = DumpCfg(len(sys.argv), sys.argv)
except Exception:
    sys.exit(1)

sys.exit(app())
