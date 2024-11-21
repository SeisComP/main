#!/usr/bin/env python

import sys
import traceback

import seiscomp.core
import seiscomp.client
import seiscomp.datamodel
import seiscomp.math

nearestStations = 300


class _Station:
    pass


class _GridPoint:
    pass


def readGrid(gridfile):
    grid = []

    with open(gridfile, "r", encoding="utf8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("#"):
                continue

            line = line.split()

            p = _GridPoint()
            p.lat, p.lon, p.dep, p.rad, p.dist = tuple(map(float, line[:5]))
            p.nmin = int(line[5])

            grid.append(p)

    return grid


def writeGrid(grid, gridfile):
    with open(gridfile, "w", encoding="utf8") as f:
        for p in grid:
            dist = min(int(p.dist + 1), 180)
            print(
                "%6.2f %6.2f %5.1f %5.2f %5.1f %d"
                % (p.lat, p.lon, p.dep, p.rad, dist, p.nmin),
                file=f,
            )


class InvApp(seiscomp.client.Application):
    def __init__(self, argc, argv):
        seiscomp.client.Application.__init__(self, argc, argv)
        self.setMessagingEnabled(False)
        self.setDatabaseEnabled(True, True)
        self.setLoggingToStdErr(True)

    def validateParameters(self):
        try:
            if not seiscomp.client.Application.validateParameters(self):
                return False

        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)

        return True

    def run(self):

        grid = readGrid("config/grid.conf")

        now = seiscomp.core.Time.GMT()
        try:
            _stations = []
            dbr = seiscomp.datamodel.DatabaseReader(self.database())
            inv = seiscomp.datamodel.Inventory()
            dbr.loadNetworks(inv)
            nnet = inv.networkCount()
            for inet in range(nnet):
                net = inv.network(inet)
                dbr.load(net)
                nsta = net.stationCount()
                for ista in range(nsta):
                    sta = net.station(ista)
                    # line = "%-2s %-5s %9.4f %9.4f %6.1f" % (
                    #     net.code(),
                    #     sta.code(),
                    #     sta.latitude(),
                    #     sta.longitude(),
                    #     sta.elevation(),
                    # )

                    try:
                        start = sta.start()
                    except:
                        continue

                    try:
                        end = sta.end()
                        if not start <= now <= end:
                            continue
                    except:
                        pass

                    _sta = _Station()
                    _sta.code = sta.code()
                    _sta.net = net.code()
                    _sta.lat, _sta.lon = sta.latitude(), sta.longitude()

                    _stations.append(_sta)

            for p in grid:
                distances = []
                for s in _stations:
                    d = seiscomp.math.delta(p.lat, p.lon, s.lat, s.lon)
                    distances.append(d)
                distances.sort()
                if len(distances) >= nearestStations:
                    p.dist = distances[nearestStations]
                else:
                    p.dist = 180

            writeGrid(grid, "config/grid.conf.new")

            return True

        except:
            info = traceback.format_exception(*sys.exc_info())
            for i in info:
                sys.stderr.write(i)
            sys.exit(-1)


def main():
    app = InvApp(len(sys.argv), sys.argv)
    app()


if __name__ == "__main__":
    main()
