#!/usr/bin/env python
# -*- coding: utf-8 -*-

import seiscomp.datamodel
import seiscomp.autoloc
import seiscomp.logging
import seiscomp.client
import seiscomp.io

seiscomp.logging.enableConsoleLogging(seiscomp.logging.getAll())


class MyAutoloc(seiscomp.autoloc.Autoloc3):
    def _report(self, origin):
        seiscomp.logging.info("MyAutoloc::_report "+origin.publicID())
        return True


def readEventParametersFromXML(xmlFile="-"):
    """
    Reads an EventParameters root element from a SeisComP XML file.
    The XML file may be gzip'ped.

    The returned EventParameters instance holds all picks, amplitudes
    and origins as child elements.
    """
    ar = seiscomp.io.XMLArchive()
    if xmlFile.lower().endswith(".gz"):
        ar.setCompression(True)
        ar.setCompressionMethod(seiscomp.io.XMLArchive.GZIP)
    if ar.open(xmlFile) is False:
        raise IOError(xmlFile + ": unable to open")
    obj = ar.readObject()
    if obj is None:
        raise TypeError(xmlFile + ": invalid format")
    ep  = seiscomp.datamodel.EventParameters.Cast(obj)
    if ep is None:
        raise TypeError(xmlFile + ": no eventparameters found")
    return ep


def test_config():
    c = seiscomp.autoloc.Config()
    assert c.amplTypeAbs == "mb"
    assert c.amplTypeSNR == "snr"

    assert len(c.authors) == 0
    c.authors.append("author1")
    c.authors.append("author2")
    c.authors.append("author3")
    assert len(c.authors) == 3

    assert [a for a in c.authors] == ['author1', 'author2', 'author3']

    assert c.adoptManualDepth == False


def test_stationLocationFile():
    s = """
    AA STA 1.23 12.34 111.1
    BB STB 2.34 23.45 222.2
    CC STC 3.45 34.56 333.3
    DD STD 4.56 45.67 444.4
    EE STE 5.67 56.78 555.5
    EE STX 6.78 67.89 555.5
    EE STY 7.89 78.90 555.5
    """

    filename = "/tmp/test-stationLocationFile.txt"
    f = open(filename, "w")
    f.write(s)
    f.close()

    inventory = seiscomp.autoloc.inventoryFromStationLocationFile(filename)
    # for this to work it is important in autoloc.i to
    # %import  "seiscomp/datamodel/inventory.h"
    # rather than %include
    assert type(inventory) == seiscomp.datamodel.Inventory

    assert inventory.networkCount() == 5
    network = inventory.network(0)
    assert type(network) == seiscomp.datamodel.Network
    assert network.stationCount() == 1
    assert network.code() == "AA"
    station = network.station(0)
    assert type(station) == seiscomp.datamodel.Station
    assert station.code() == "STA"
    assert station.latitude()  == 1.23
    assert station.longitude() == 12.34
    assert station.elevation() == 111.1

    network = inventory.network(4)
    assert network.stationCount() == 3


def test_stationConfig():
    s = """
    *  *     1  90.0
    AA *     1  60.0
    AA STA   0  60.0
    XX *     0  30.0
    XX STX   1  30.0
    """

    filename = "/tmp/test-stationConfigFile.txt"
    f = open(filename, "w")
    f.write(s)
    f.close()

    c = seiscomp.autoloc.StationConfig()
    c.setFilename(filename)
    c.read() 

    sc = c.get("AA","STA")
    assert (sc.usage, sc.maxNucDist) == (0, 60)
    sc = c.get("AA","STA2")
    assert (sc.usage, sc.maxNucDist) == (1, 60)
    sc = c.get("XX","STX")
    assert (sc.usage, sc.maxNucDist) == (1, 30)
    sc = c.get("XX","STX2")
    assert (sc.usage, sc.maxNucDist) == (0, 30)

    # default
    sc = c.get("BB","STB")
    assert (sc.usage, sc.maxNucDist) == (1, 90)


def test_PublicObjectQueue(verbose=False):
    q = seiscomp.autoloc.PublicObjectQueue()
    assert q.empty()
    assert q.size() == 0

    # this takes about 10 seconds!
    ep = readEventParametersFromXML(baseDir + "/objects.xml.gz")
    q.fill(ep)
    assert not q.empty()
    assert q.size() > 1000

    objects = q
    o = objects.front()
    o = seiscomp.datamodel.Pick.Cast(o)
    assert o.referenceCount() == 3 # ep, objects, Python wrapper
    objects.pop()
    assert o.referenceCount() == 2 # ep, Python wrapper
    del ep
    assert o.referenceCount() == 1 # Python wrapper


def test_run(verbose=False):

    stationLocationFile = baseDir+"/config/station-locations.txt"
    stationConfigFile = baseDir+"/config/station.conf"
    gridConfigFile = baseDir+"/config/grid.conf"

    # generate SeisComP Inventory from plain-text location table
    inventory = seiscomp.autoloc.inventoryFromStationLocationFile(stationLocationFile)

    autoloc = MyAutoloc()
    if verbose:
        print(scautoloc) 

    # general config
    config = seiscomp.autoloc.Config()
    config.agencyID = "GFZ"
    config.author = "autoloc-test"
    config.offline = True
    config.playback = True
    config.useImportedOrigins = False
    config.useManualOrigins = False
    config.adoptManualDepth = False
    config.tryDefaultDepth = True
    config.cleanupInterval = 300
    config.keepPick = 86400
    config.stationConfig = stationConfigFile
    autoloc.setConfig(config)
    assert config.check()

    # SeisComP Inventory
    autoloc.setInventory(inventory)

    # Station config
    # autoloc.setStationConfigFilename(stationConfigFile)
    # TODO:
    # c = seiscomp.autoloc.StationConfig()
    # c.setFilename(filename)
    # c.read()
    # autoloc.setStationConfig(c)

    # Grid file used in the grid search (nucleator)
    autoloc.setGridFilename(gridConfigFile)

    OK = autoloc.init()
    assert OK

    autoloc.dumpConfig()

    # now read the parametric data

    objects = seiscomp.autoloc.PublicObjectQueue()
    # this takes about 10 seconds!
    ep = readEventParametersFromXML(baseDir + "/objects.xml.gz")
    objects.fill(ep)

    for o in objects:
#       seiscomp.logging.debug("Working on object queue. %d objects remain." % (objects.size()))
        autoloc.sync(o.creationInfo().creationTime())
        autoloc.feed(o)


if __name__ == "__main__":
#   test_config()
#   test_stationLocationFile()
#   test_stationConfig()
#   test_PublicObjectQueue()
    test_run()
