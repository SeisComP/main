#!/usr/bin/env python3
"""
Pure-Python DepthLookup — mirrors the C++ class API.

Standalone depth lookup from USGS Slab2.0 footprint polygons and/or
manually drawn BNA region polygons.  No SeisComP installation required.

Dependencies:
    pip install shapely numpy

Usage
-----
    from depthlookup import DepthLookup

    ds = DepthLookup()
    ds.load_slab_contours('/path/to/scautoloc/slabs')
    ds.load_regions(['/path/to/regions.bna'])   # optional manual regions

    print(ds.get_default_depth(37.5,   22.0,  10.0))  # Hellenic arc → ~50
    print(ds.get_default_depth(-20.0, 178.0,  10.0))  # Tonga → ~200+
    print(ds.get_default_depth(52.0,   13.0,  10.0))  # Germany → 10 (fallback)

Priority order (same as C++ class):
    1. Slab contours  — deepest Slab2.0 footprint polygon containing the point
    2. Manual regions — defaultDepth attribute from a BNA region polygon
    3. fallback       — caller-supplied value

Slab directory layout (produced by generate_slab_bna.py):
    <slabs>/000/<zone>.bna
    <slabs>/020/<zone>.bna
    ...
    <slabs>/680/<zone>.bna

Manual region BNA format:
    "My Region","defaultDepth=15 maxDepth=200",-N
    lon1 , lat1
    lon2 , lat2
    ...
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import numpy as np
from shapely.geometry import MultiPolygon, Point, Polygon
from shapely.ops import unary_union


# ---------------------------------------------------------------------------
# Internal data structures
# ---------------------------------------------------------------------------

@dataclass
class _SlabLevel:
    depth_km: int
    polygons: List  # list of shapely Polygon/MultiPolygon


@dataclass
class _SlabZone:
    name: str
    levels: List[_SlabLevel] = field(default_factory=list)  # ascending by depth_km
    max_depth_km: int = 0


@dataclass
class _Region:
    name: str
    polygon: object        # shapely geometry
    default_depth: float
    max_depth: Optional[float]


# ---------------------------------------------------------------------------
# Main class
# ---------------------------------------------------------------------------

class DepthLookup:
    """
    Region- and slab-based default/maximum depth lookup.

    Mirrors the C++ DepthLookup in scautoloc so that depth-logic
    scenarios can be tested in a minimal Python environment.
    """

    def __init__(self):
        self._slab_zones: List[_SlabZone] = []
        self._regions:    List[_Region]   = []

    # ------------------------------------------------------------------ #
    # Loading                                                              #
    # ------------------------------------------------------------------ #

    def load_slab_contours(self, directory: str) -> int:
        """
        Load slab depth-footprint BNA files from a directory tree.

        Scans numeric depth subdirectories (000, 020, …, 680), reads every
        .bna file, groups polygons by zone name, and sorts them by depth.
        Returns the number of zones loaded.
        """
        if not os.path.isdir(directory):
            raise FileNotFoundError(f'Slab directory not found: {directory}')

        zone_map: Dict[str, _SlabZone] = {}

        depth_dirs = sorted(
            d for d in os.listdir(directory)
            if os.path.isdir(os.path.join(directory, d)) and d.isdigit()
        )

        for depth_dir in depth_dirs:
            depth_km  = int(depth_dir)
            dir_path  = os.path.join(directory, depth_dir)

            for fname in sorted(os.listdir(dir_path)):
                if not fname.endswith('.bna'):
                    continue

                zone_name = fname[:-4]
                bna_path  = os.path.join(dir_path, fname)

                try:
                    polys = _read_bna_polygons(bna_path)
                except Exception as e:
                    print(f'Warning: {bna_path}: {e}', file=sys.stderr)
                    continue

                if not polys:
                    continue

                if zone_name not in zone_map:
                    zone_map[zone_name] = _SlabZone(name=zone_name)
                zone_map[zone_name].levels.append(
                    _SlabLevel(depth_km=depth_km, polygons=polys)
                )

        for zone in zone_map.values():
            zone.levels.sort(key=lambda lv: lv.depth_km)
            zone.max_depth_km = zone.levels[-1].depth_km if zone.levels else 0

        self._slab_zones = list(zone_map.values())
        return len(self._slab_zones)

    def load_regions(self, paths) -> int:
        """
        Load manual depth-region polygons from BNA files.

        Each BNA record must have defaultDepth (and optionally maxDepth)
        in its attribute field:
            "My Region","defaultDepth=15 maxDepth=200",-N
            lon1 , lat1
            ...

        paths: a single path string, or a list of paths.
               Directories are scanned for .bna files.
        Returns number of region polygons loaded.
        """
        if isinstance(paths, str):
            paths = [paths]

        bna_files = []
        for p in paths:
            if os.path.isdir(p):
                bna_files += sorted(
                    os.path.join(p, f) for f in os.listdir(p)
                    if f.endswith('.bna')
                )
            elif os.path.isfile(p):
                bna_files.append(p)

        loaded = 0
        for bna_path in bna_files:
            try:
                records = _read_bna_records(bna_path)
            except Exception as e:
                print(f'Warning: {bna_path}: {e}', file=sys.stderr)
                continue

            for name, attrs, lons, lats in records:
                dd = _parse_attr(attrs, 'defaultDepth')
                if dd is None:
                    continue
                md   = _parse_attr(attrs, 'maxDepth')
                poly = _make_polygon(lons, lats)
                if poly is None:
                    continue
                self._regions.append(
                    _Region(name=name, polygon=poly,
                            default_depth=dd, max_depth=md)
                )
                loaded += 1

        return loaded

    # ------------------------------------------------------------------ #
    # Depth lookup                                                         #
    # ------------------------------------------------------------------ #

    def get_default_depth(self, lat: float, lon: float,
                          fallback: float) -> float:
        """
        Return default depth (km) at (lat, lon).

        Tries slab contours first, then manual regions, then fallback.
        """
        slab_depth = self._slab_depth_at(lat, lon)
        if slab_depth >= 0:
            return float(slab_depth)

        constraint = self._match_region(lat, lon)
        if constraint is not None:
            return constraint.default_depth

        return fallback

    def get_max_depth(self, lat: float, lon: float,
                      fallback: float) -> float:
        """
        Return maximum acceptable depth (km) at (lat, lon).

        For slab zones: returns the zone's deepest contour level.
        For manual regions: returns the maxDepth attribute if set.
        Falls back to fallback if neither applies.
        """
        for zone in self._slab_zones:
            if zone.levels and _polygons_contain(zone.levels[0].polygons, lat, lon):
                return float(zone.max_depth_km)

        constraint = self._match_region(lat, lon)
        if constraint is not None and constraint.max_depth is not None:
            return constraint.max_depth

        return fallback

    # ------------------------------------------------------------------ #
    # Introspection                                                        #
    # ------------------------------------------------------------------ #

    def has_slabs(self)   -> bool: return bool(self._slab_zones)
    def has_regions(self) -> bool: return bool(self._regions)
    def zone_count(self)  -> int:  return len(self._slab_zones)
    def region_count(self)-> int:  return len(self._regions)

    def zone_names(self) -> List[str]:
        return sorted(z.name for z in self._slab_zones)

    def zone_info(self, name: str) -> Optional[Tuple[int, int]]:
        """Return (min_depth_km, max_depth_km) for a zone, or None."""
        for z in self._slab_zones:
            if z.name == name:
                return (z.levels[0].depth_km, z.max_depth_km) if z.levels else None
        return None

    # ------------------------------------------------------------------ #
    # Private                                                              #
    # ------------------------------------------------------------------ #

    def _slab_depth_at(self, lat: float, lon: float) -> int:
        """
        Return the deepest slab depth level whose footprint contains (lat,lon).
        Returns -1 if the point is not inside any slab zone.
        """
        for zone in self._slab_zones:
            # scan deepest → shallowest; return first match
            for level in reversed(zone.levels):
                if _polygons_contain(level.polygons, lat, lon):
                    return level.depth_km
        return -1

    def _match_region(self, lat: float, lon: float) -> Optional['_Region']:
        for region in self._regions:
            if _polygon_contains(region.polygon, lat, lon):
                return region
        return None


# ---------------------------------------------------------------------------
# BNA I/O
# ---------------------------------------------------------------------------

def _read_bna_records(filepath: str):
    """
    Parse a BNA file and yield (name, attrs, lons, lats) for every record.
    """
    records = []
    with open(filepath, 'r') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if not line:
            continue

        # Header: "name","attrs",±N
        m = re.match(r'"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(-?\d+)', line)
        if not m:
            continue

        name       = m.group(1)
        attrs      = m.group(2)
        n_vertices = abs(int(m.group(3)))

        lons, lats = [], []
        for _ in range(n_vertices):
            if i >= len(lines):
                break
            parts = lines[i].strip().split(',')
            i += 1
            if len(parts) >= 2:
                try:
                    lons.append(float(parts[0]))
                    lats.append(float(parts[1]))
                except ValueError:
                    pass

        records.append((name, attrs, lons, lats))

    return records


def _read_bna_polygons(filepath: str) -> List:
    """Read all polygons from a slab BNA file as shapely geometry objects."""
    records = _read_bna_records(filepath)
    polygons = []
    for _, _, lons, lats in records:
        poly = _make_polygon(lons, lats)
        if poly is not None:
            polygons.append(poly)
    return polygons


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _make_polygon(lons, lats) -> Optional[object]:
    """
    Build a shapely Polygon from BNA vertex lists.

    Polygons that span the anti-meridian (have both lon > 170 and lon < -170)
    are normalised to 0-360 convention so that shapely treats them as a single
    connected region.  _polygon_contains() handles the matching normalisation
    for query points.
    """
    if len(lons) < 3:
        return None

    lons = np.array(lons, dtype=float)
    lats = np.array(lats, dtype=float)

    # Anti-meridian crossing: normalise to 0-360
    if lons.max() > 170 and lons.min() < -170:
        lons = np.where(lons < 0, lons + 360.0, lons)

    try:
        poly = Polygon(zip(lons, lats))
        if not poly.is_valid:
            poly = poly.buffer(0)   # heal self-intersections
        if poly.is_empty:
            return None
        return poly
    except Exception:
        return None


def _polygon_contains(polygon, lat: float, lon: float) -> bool:
    """
    Test point (lat, lon) against a shapely polygon or multipolygon,
    respecting the 0-360 normalisation applied in _make_polygon() for
    anti-meridian crossing polygons.
    """
    # Get all exterior rings to check the coordinate convention used
    if hasattr(polygon, 'geoms'):
        exteriors = [g.exterior for g in polygon.geoms]
    else:
        exteriors = [polygon.exterior]

    poly_lons = np.concatenate([np.array(e.coords)[:, 0] for e in exteriors])

    # If polygon was stored in 0-360 space, shift query point accordingly
    query_lon = (lon + 360.0) if (poly_lons.max() > 180 and lon < 0) else lon

    return polygon.contains(Point(query_lon, lat))


def _polygons_contain(polygons: list, lat: float, lon: float) -> bool:
    """Return True if (lat, lon) is inside any polygon in the list."""
    return any(_polygon_contains(p, lat, lon) for p in polygons)


def _parse_attr(attrs: str, key: str) -> Optional[float]:
    """Extract 'key=value' from an attribute string."""
    m = re.search(rf'\b{re.escape(key)}=([+-]?\d+(?:\.\d+)?)', attrs)
    return float(m.group(1)) if m else None


# ---------------------------------------------------------------------------
# Command-line interface
# ---------------------------------------------------------------------------

def _parse_args():
    p = argparse.ArgumentParser(
        description='Query slab/region depth at a geographic point.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Single point query
  python3 depthlookup.py --slabs /opt/seiscomp/share/dlslab2/slabs \\
      --lat 37.5 --lon 22.0

  # Batch query from CSV (lat,lon per line, printed to stdout)
  python3 depthlookup.py --slabs /opt/seiscomp/share/dlslab2/slabs \\
      --batch points.csv
"""
    )
    p.add_argument('--slabs',   help='Slab BNA directory (depth subdirs)')
    p.add_argument('--regions', nargs='+', help='Manual region BNA files/dirs')
    p.add_argument('--lat',     type=float, help='Latitude')
    p.add_argument('--lon',     type=float, help='Longitude')
    p.add_argument('--fallback',type=float, default=10.0,
                   help='Fallback depth in km (default: 10)')
    p.add_argument('--batch',   help='CSV file with lat,lon per line')
    p.add_argument('--zones',   action='store_true',
                   help='List loaded zone names and depth ranges')
    return p.parse_args()


def main():
    args = _parse_args()

    ds = DepthLookup()

    if args.slabs:
        n = ds.load_slab_contours(args.slabs)
        print(f'Loaded {n} slab zones', file=sys.stderr)

    if args.regions:
        n = ds.load_regions(args.regions)
        print(f'Loaded {n} manual regions', file=sys.stderr)

    if args.zones:
        print(f'\n{"Zone":<25}  min   max (km)')
        print('-' * 42)
        for name in ds.zone_names():
            info = ds.zone_info(name)
            if info:
                print(f'{name:<25}  {info[0]:4d}  {info[1]:4d}')
        print()

    if args.batch:
        print('lat,lon,default_depth,max_depth')
        with open(args.batch) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split(',')
                lat, lon = float(parts[0]), float(parts[1])
                dd = ds.get_default_depth(lat, lon, args.fallback)
                md = ds.get_max_depth(lat, lon, args.fallback)
                print(f'{lat},{lon},{dd},{md}')

    elif args.lat is not None and args.lon is not None:
        dd = ds.get_default_depth(args.lat, args.lon, args.fallback)
        md = ds.get_max_depth(args.lat,     args.lon, args.fallback)
        print(f'lat={args.lat}  lon={args.lon}')
        print(f'  default depth : {dd} km')
        print(f'  max depth     : {md} km')


if __name__ == '__main__':
    main()
