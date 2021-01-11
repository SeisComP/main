from __future__ import print_function
import os
import string
import time
import re
import glob
import shutil
import sys
import imp
import random
import fnmatch
import seiscomp.core
import seiscomp.config
import seiscomp.kernel
import seiscomp.system
import seiscomp.client
import seiscomp.datamodel


DEBUG = 0


def parseBindPort(bind):
    bindToks = bind.split(':')
    if len(bindToks) == 1:
        return int(bindToks[0])
    elif len(bindToks) == 2:
        return int(bindToks[1])
    else:
        return -1


def collectParams(container):
    params = {}

    for i in range(container.groupCount()):
        params.update(collectParams(container.group(i)))

    for i in range(container.structureCount()):
        params.update(collectParams(container.structure(i)))

    for i in range(container.parameterCount()):
        p = container.parameter(i)

        if p.symbol.stage == seiscomp.system.Environment.CS_UNDEFINED:
            continue

        params[p.variableName] = ",".join(p.symbol.values)

    return params

def logd(message):
    '''
    Debugging method
    '''
    if DEBUG:
        print(message, file=sys.stderr)
        sys.stderr.flush()

def log(message):
    '''
    Helper method for outputting with flushing
    '''
    print(message, file=sys.stdout)
    sys.stdout.flush()

class InventoryResolver(object):
    def __init__(self, inventory):
        self._inventory = inventory
        pass

    '''
        Those should be internal methods only 
    '''
    def _overlaps(self, pstart, pend, cstart, cend):
        if cstart is None and cend is None: return True
        
        if cstart is None:
            cstart = seiscomp.core.Time()
        
        if pend is not None:
            if pend > cstart:
                if cend is None or pstart < cend:
                    return True
        else:
            if cend is None or pstart < cend:
                return True
    
        return False

    def _getEnd(self, obj):
        try:
            return obj.end()
        except ValueError:
            return None

    def _codeMatch(self, obj, code):
        if not code: return True
        if fnmatch.fnmatch(str(obj.code()).upper(), code.strip().upper()): return True
        return False

    def _collect(self, objs, count, code, start, end):
        items = []

        for i in range(0, count):
            obj = objs(i)
    
            # Check code
            if not self._codeMatch(obj, code): continue 
    
            # Check time
            if not self._overlaps(obj.start(), self._getEnd(obj), start, end): continue
    
            items.append(obj)

        return items

    def _findStreams(self, location, code, start, end):
        items = self._collect(location.stream, location.streamCount(), code, start, end)
        if len(items) == 0:
            raise Exception("Location %s / %s does not have a stream named: %s in the time range %s / %s " % (location.code(), location.start(), code, start, end))
        return items

    def _findLocations(self, station, code, start, end):
        items = self._collect(station.sensorLocation, station.sensorLocationCount(), code, start, end)
        if len(items) == 0:
            raise Exception("Station %s / %s does not have a location named: %s in the time range %s / %s " % (station.code(), station.start(), code, start, end))
        return items

    def _findStations(self, network, code, start, end):
        items = self._collect(network.station, network.stationCount(), code, start, end)
        if len(items) == 0:
            raise Exception("Network %s / %s does not have a station named: %s in the time range %s / %s " % (network.code(), network.start(), code, start, end))
        return items

    def _findNetworks(self, code, start, end):
        items = self._collect(self._inventory.network, self._inventory.networkCount(), code, start, end)
        if len(items) == 0:
            raise Exception("Inventory does not have a network named: %s in the time range %s / %s " % (code, start, end))
        return items

    def _truncateDate(self, obj, currentDate):
        if currentDate < obj.start():
            return obj.start()
        end = self._getEnd(obj)
        if end and currentDate > end:
            return end
        return currentDate

    '''
        Public methods that should be used 
    '''
    def findStartDate(self, network, start, end):
        if start is None:
            return network.start()
        return self._truncateDate(network, start)

    def findEndDate(self, network, start, end):
        if end is None:
            try: return network.end()
            except ValueError: return None

        return self._truncateDate(network, end)

    def expandStream(self, stations, streams, start, end):
        items = []
    
        for strm in streams.split(','):
            (locationCode, streamCode) = ('.' + strm).split('.')[-2:]
            for station in stations:
                try:
                    for location in self._findLocations(station, locationCode, start, end):
                        if locationCode:
                            currentLocCode = location.code()
                        else:
                            currentLocCode = ""
                        try:
                            for stream in self._findStreams(location, streamCode, start, end):
                                try:
                                    items.index((currentLocCode, stream.code()))
                                except:
                                    items.append((currentLocCode, stream.code()))
                        except Exception as e:
                            pass
                except Exception as e:
                    pass
    
        return items

    def expandNetworkStation(self, ncode, scode, start, end):
        items = []

        for network in self._findNetworks(ncode, start, end):

            try:
                stations = self._findStations(network, scode, start, end)
            except Exception as  e:
                logd(str(e))
                continue

            # Append
            items.append((network, stations))
    
        if len(items) == 0:
            raise Exception("Cannot find suitable %s network with station code %s ranging from %s / %s" % (ncode, scode, start, end))
        return items

class AccessUpdater(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setLoggingToStdErr(True)
        self.setMessagingEnabled(True)
        self.setDatabaseEnabled(True, True)
        self.setAutoApplyNotifierEnabled(False)
        self.setInterpretNotifierEnabled(False)
        self.setMessagingUsername("_sccfgupd_")
        ##self.setLoadConfigModuleEnabled(True)
        # Load all configuration modules
        ##self.setConfigModuleName("")
        self.setPrimaryMessagingGroup(seiscomp.client.Protocol.LISTENER_GROUP)

    def send(self, *args):
        '''
        A simple wrapper that sends a message and tries to resend it in case of
        an error.
        '''
        while not self.connection().send(*args):
            log("sending failed, retrying")
            time.sleep(1)

    def sendNotifiers(self, group):
        Nsize = seiscomp.datamodel.Notifier.Size()

        if Nsize > 0:
            logd("trying to apply %d changes..." % Nsize)
        else:
            logd("no changes to apply")
            return

        Nmsg = seiscomp.datamodel.Notifier.GetMessage(True)

        it = Nmsg.iter()
        msg = seiscomp.datamodel.NotifierMessage()

        maxmsg = 100
        sent = 0
        mcount = 0

        try:
            try:
                while it.get():
                    msg.attach(seiscomp.datamodel.Notifier.Cast(it.get()))
                    mcount += 1
                    if msg and mcount == maxmsg:
                        sent += mcount
                        logd("sending message (%5.1f %%)" % (sent / float(Nsize) * 100.0))
                        self.send(group, msg)
                        msg.clear()
                        mcount = 0
                        #self.sync("_sccfgupd_")

                    it.next()
            except:
                pass
        finally:
            if msg.size():
                logd("sending message (%5.1f %%)" % 100.0)
                self.send(group, msg)
                msg.clear()
                #self.sync("_sccfgupd_")

    def run(self):
        '''
        Reimplements the main loop of the application. This methods collects
        all bindings and updates the database. It searches for already existing
        objects and updates them or creates new objects. Objects that is didn't
        touched are removed. This tool is the only one that should writes the
        configuration into the database and thus manages the content.
        '''
        # Initialize the basic directories
        filebase = seiscomp.system.Environment.Instance().installDir()
        descdir = os.path.join(filebase, "etc", "descriptions")
        keydir = os.path.join(filebase, "etc", "key", self.name())

        # Load definitions of the configuration schema
        defs = seiscomp.system.SchemaDefinitions()
        if defs.load(descdir) == False:
            log("could not read descriptions")
            return False

        if defs.moduleCount() == 0:
            log("no modules defined, nothing to do")
            return False

        # Create a model from the schema and read its configuration including
        # all bindings.
        model = seiscomp.system.Model()
        model.create(defs)
        model.readConfig()

        mod_access = model.module("access")

        existingAccess  = {}

        routing = self.query().loadRouting()
        inventory = self.query().loadInventory()
        iResolver = InventoryResolver(inventory)

        seiscomp.datamodel.Notifier.Enable()
        seiscomp.datamodel.Notifier.SetCheckEnabled(False)

        # Update access on basis of access module
        if mod_access:
            logd("Working on access bindings")
            for staid in mod_access.bindings.keys():
                binding = mod_access.getBinding(staid)
                if not binding: continue

                params = {}
                for i in range(binding.sectionCount()):
                    params.update(collectParams(binding.section(i)))

                access_users = params.get('access.users')
                access_start = params.get('access.start')
                access_end = params.get('access.end')
                access_netonly = params.get('access.disableStationCode')
                access_streams = params.get('access.streams')

                if access_netonly is None or access_netonly == "false":
                    access_netonly = False
                else:
                    access_netonly = True

                if not access_users: continue

                networkCode = staid.networkCode
                stationCode = staid.stationCode

                if access_start:
                    access_start = seiscomp.core.Time.FromString(access_start, "%Y-%m-%d %H:%M:%S")

                if access_end:
                    access_end = seiscomp.core.Time.FromString(access_end, "%Y-%m-%d %H:%M:%S")

                if access_netonly:
                    stationCode = ""
                
                ## Resolve Inventory
                try:
                    networkList = iResolver.expandNetworkStation(networkCode, stationCode, access_start, access_end)
                except Exception as  e:
                    #log("Access issue, cannot find network object for %s %s::\n\t %s" % (staid.networkCode, staid.stationCode, str(e)))
                    for user in access_users.split(','):
                        existingAccess[(networkCode, "", "", "", user, "1980-01-01 00:00:00")] = (None,)
                    continue

                ## Generate routes for each network found
                for (network, stations) in networkList:
                    
                    ## Resolve start date / end date of routing to be generated
                    aStart = iResolver.findStartDate(network, access_start, access_end)
                    aEnd   = iResolver.findEndDate(network, access_start, access_end)

                    if not access_streams:
                        for user in access_users.split(','):
                            existingAccess[(networkCode, stationCode, "", "", user, aStart.toString("%Y-%m-%d %H:%M:%S"))] = (aEnd,)
                        continue
                    
                    ## Add the route or routes for this net
                    for (locationCode, streamCode) in iResolver.expandStream(stations, access_streams, access_start, access_end):
                        for user in access_users.split(','):
                            existingAccess[(networkCode, stationCode, locationCode, streamCode, user, aStart.toString("%Y-%m-%d %H:%M:%S"))] = (aEnd,)


        for ((networkCode, stationCode, locationCode, streamCode, user, start), (end,)) in existingAccess.items():
            access = routing.access(seiscomp.datamodel.AccessIndex(networkCode, stationCode, locationCode, streamCode, user, seiscomp.core.Time.FromString(start, "%Y-%m-%d %H:%M:%S")))
            if not access:
                access = seiscomp.datamodel.Access()
                access.setNetworkCode(networkCode)
                access.setStationCode(stationCode)
                access.setLocationCode(locationCode)
                access.setStreamCode(streamCode)
                access.setUser(user)
                access.setStart(seiscomp.core.Time.FromString(start, "%Y-%m-%d %H:%M:%S"))
                access.setEnd(end)
                routing.add(access)
            else:
                update = False
                try:
                    cend = access.end()
                    if (not end) or (end and cend != end):
                        access.setEnd(end)
                        update = True
                except ValueError as e:
                    if end:
                        access.setEnd(end)
                        update = True
                
                if update:
                    access.update()


        i = 0
        while i < routing.accessCount():
            access = routing.access(i)
            if (access.networkCode(), access.stationCode(), access.locationCode(), access.streamCode(), access.user(), access.start().toString("%Y-%m-%d %H:%M:%S")) not in existingAccess:
                routing.remove(access)
                continue

            i += 1

        self.sendNotifiers("ROUTING")
        return True

class Module(seiscomp.kernel.Module):
    def __init__(self, env):
        seiscomp.kernel.Module.__init__(self, env, env.moduleName(__file__))

    def start(self):
        return 0

    def updateConfig(self):
        messaging = True
        messagingPort = 18180
        messagingProtocol = 'scmp';

        try: messaging = self.env.getBool("messaging.enable")
        except: pass

        # If messaging is disabled in kernel.cfg, do not do anything
        if not messaging:
            log("- messaging disabled, nothing to do")
            return 0

        # Load scmaster configuration and figure the bind ports of scmaster out
        cfg = seiscomp.config.Config()
        seiscomp.system.Environment.Instance().initConfig(cfg, "scmaster")

        # First check the unencrypted port and prefer that
        p = parseBindPort(cfg.getString("interface.bind"))
        if p > 0:
            messagingPort = p

            try:
                bind = self.env.getString("messaging.bind")
                bindToks = bind.split(':')
                if len(bindToks) == 1:
                    messagingPort = int(bindToks[0])
                elif len(bindToks) == 2:
                    messagingPort = int(bindToks[1])
                else:
                    sys.stdout.write(
                        "E invalid messaging bind parameter: %s\n" % bind)
                    sys.stdout.write("  expected either 'port' or 'ip:port'\n")
                    return 1
            except:
                pass

        # Otherwise check if ssl is enabled
        else:
            p = parseBindPort(cfg.getString("interface.ssl.bind"))
            if p > 0:
                messagingPort = p
                messagingProtocol = 'scmps'

        # Synchronize database configuration
        params = [self.name, '--console', '1', '-H',
                  '%s://localhost:%d' % (messagingProtocol, messagingPort)]
        # Create the database update app and run it
        # This app implements a seiscomp.client.Application and connects
        # to localhost regardless of connections specified in global.cfg to
        # prevent updating a remote installation by accident.
        app = AccessUpdater(len(params), params)
        return app()
