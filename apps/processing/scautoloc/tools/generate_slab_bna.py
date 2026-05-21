#!/usr/bin/env python3
"""
Generate Slab2.0 depth-footprint BNA files for scautoloc DepthLookup.

For each subduction zone and each depth level (0, 20, 40, ..., 700 km),
produces a BNA polygon representing the geographic footprint of the slab
at that depth or deeper. A point inside `hellenic_100.bna` means the
Hellenic slab surface is at least 100 km deep at that location.

Usage:
    # Primary: USGS Slab2.0 final output grids (Hayes et al. 2018)
    python3 generate_slab_bna.py \\
        --input   /path/to/Slab2Distribute_Mar2018 \\
        --output  /path/to/output/slabs

    # Also include guide-file supplements for zones absent from the
    # final release (1car/lesser_antilles, exp/explorer, pan/panama):
    python3 generate_slab_bna.py \\
        --input   /path/to/Slab2Distribute_Mar2018 \\
        --input-guide /path/to/usgs-slab-models/src/slabguides \\
        --output  /path/to/output/slabs

Output structure:
    slabs/000/hellenic.bna
    slabs/020/hellenic.bna
    ...

Input .grd files are USGS NetCDF grids, variables: x (lon), y (lat),
z (depth, negative km below surface). Some grids use 0-360° longitude
convention; normalisation to -180/180 is done after contour extraction
so that np.interp operates on a monotonic axis (important for grids
spanning the anti-meridian, e.g. Aleutians 165-222°, Kermadec 170-188°).
"""

import argparse
import os
import sys

import netCDF4 as nc
import numpy as np
from skimage import measure


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEPTH_LEVELS_KM = list(range(0, 720, 20))   # 0, 20, 40, ..., 700

# Minimum number of grid cells in a polygon — filters out tiny fragments
# (~20 cells ≈ 50 km² at 0.05° resolution, enough to be geographically meaningful)
MIN_CELLS = 20

# Minimum number of vertices to keep a contour
MIN_VERTICES = 10

# ---------------------------------------------------------------------------
# Zone name mappings
# ---------------------------------------------------------------------------

# Maps USGS zone code → readable output name for the 27-zone final Slab2.0
# release (Hayes et al. 2018, Slab2Distribute_Mar2018).
# Filename pattern: <code>_slab2_dep_<date>.grd
ZONE_NAMES_SLAB2 = {
    'alu': 'aleutians',
    'cal': 'calabria',
    'cam': 'central_america',
    'car': 'caribbean',
    'cas': 'cascadia',
    'cot': 'cotabato',
    'hal': 'halmahera',
    'hel': 'hellenic',
    'him': 'himalaya',
    'hin': 'hindu_kush',
    'izu': 'izu_bonin',
    'ker': 'kermadec',       # covers full Kermadec-Tonga arc (lat -43 to -14)
    'kur': 'kuril',
    'mak': 'makran',
    'man': 'manila',
    'mue': 'muertos',
    'pam': 'pamir',
    'phi': 'philippines',
    'png': 'papua_new_guinea',
    'puy': 'puysegur',
    'ryu': 'ryukyu',
    'sam': 'south_america',
    'sco': 'south_sandwich',
    'sol': 'solomon_islands',
    'sul': 'sulawesi',
    'sum': 'sunda',           # covers full Sunda arc: Andaman to Flores (91-133°E)
    'van': 'vanuatu',
}

# Guide-file supplements for zones absent from the final 2018 Slab2 release.
# Filename pattern: <code>_SG_*.grd  or  <code>_<date>.grd
# (same format as slabguides/ directory)
ZONE_NAMES_GUIDE = {
    '1car': 'lesser_antilles',
    'exp':  'explorer',
    'pan':  'panama',
}


# ---------------------------------------------------------------------------
# Grid loading
# ---------------------------------------------------------------------------

def load_grid(filepath):
    """
    Load a Slab2.0 .grd file (final output or guide).

    Returns (lon, lat, depth) where depth is positive downward (km).
    Longitude is kept in original convention (may be 0-360) so that
    np.interp in footprint_contours() always operates on a monotonic axis.
    """
    ds = nc.Dataset(filepath)
    lon = np.array(ds.variables['x'][:])
    lat = np.array(ds.variables['y'][:])
    z = ds.variables['z'][:]
    if hasattr(z, 'filled'):
        z = z.filled(np.nan)
    z = np.array(z, dtype=float)
    depth = -z
    depth[depth < 0] = np.nan   # mask near-trench positive artifacts
    return lon, lat, depth


# ---------------------------------------------------------------------------
# Contour extraction
# ---------------------------------------------------------------------------

def footprint_contours(lon, lat, depth, depth_km):
    """
    Return all polygons (as lists of (lon_arr, lat_arr)) where
    slab depth >= depth_km, sorted largest-first.

    Longitude is normalised to -180/180 AFTER interpolation so that
    np.interp works correctly for grids spanning the anti-meridian
    (e.g. Aleutians 165-222°E, Kermadec 170-188°E in 0-360 convention).
    """
    mask = (depth >= depth_km) & ~np.isnan(depth)

    if mask.sum() < MIN_CELLS:
        return []

    raw_contours = measure.find_contours(mask.astype(np.float32), level=0.5)

    polygons = []
    for contour in raw_contours:
        if len(contour) < MIN_VERTICES:
            continue

        rows = contour[:, 0]
        cols = contour[:, 1]

        lons = np.interp(cols, np.arange(len(lon)), lon)
        lats = np.interp(rows, np.arange(len(lat)), lat)

        # Normalise 0-360 to -180/180 after interpolation
        lons[lons > 180] -= 360

        lons = np.append(lons, lons[0])
        lats = np.append(lats, lats[0])

        polygons.append((lons, lats))

    polygons.sort(key=lambda p: len(p[0]), reverse=True)
    return polygons


# ---------------------------------------------------------------------------
# BNA writing
# ---------------------------------------------------------------------------

def write_bna(filepath, zone_name, depth_km, polygons):
    """
    Write one or more polygons to a BNA file in gempa's slab format.

    Header:  "<zone> <DDD> km","rank 1",-<nvertices>
    Vertex:  lon , lat   (5 decimal places)

    Negative vertex count signals a closed polygon to SeisComP's BNA reader.
    Multiple polygons are written as successive blocks in the same file.
    """
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'w') as f:
        for lons, lats in polygons:
            n = len(lons)
            f.write(f'"{zone_name} {depth_km:03d} km","rank 1",{-n}\n')
            for lo, la in zip(lons, lats):
                f.write(f'{lo:.5f} , {la:.5f}\n')


# ---------------------------------------------------------------------------
# Zone code extraction
# ---------------------------------------------------------------------------

def zone_code_slab2(filename):
    """
    Extract zone code from a final Slab2 dep filename.
    e.g. 'alu_slab2_dep_02.23.18.grd' → 'alu'
    """
    return filename.split('_slab2_')[0]


def zone_code_guide(filename):
    """
    Extract zone code from a guide-file filename.
    Handles:
      '1car_SG_car.grd'      → '1car'
      'exp_SG_exp.grd'       → 'exp'
      'pan_11.03.17_10.38.grd' → 'pan'
    """
    stem = filename.replace('.grd', '')
    for key in ('1car',):
        if stem.startswith(key):
            return key
    return stem.split('_')[0]


# ---------------------------------------------------------------------------
# Per-directory processing
# ---------------------------------------------------------------------------

def process_directory(input_dir, zone_names_map, code_fn, args,
                      depth_levels, written_zones, counters):
    """
    Process all eligible .grd files in input_dir.

    zone_names_map: dict mapping code → output zone name
    code_fn: function(filename) → zone code
    written_zones: set of already-written zone names (skip duplicates)
    counters: mutable dict with 'written' and 'skipped' keys
    """
    grd_files = sorted(f for f in os.listdir(input_dir) if f.endswith('.grd'))

    for filename in grd_files:
        code = code_fn(filename)

        if code not in zone_names_map:
            continue   # silently skip — other files in the dir are not relevant

        zone_name = zone_names_map[code]

        if zone_name in written_zones:
            print(f'  [skip] {filename} — zone "{zone_name}" already written',
                  file=sys.stderr)
            continue

        if args.zone and zone_name not in args.zone:
            continue

        filepath = os.path.join(input_dir, filename)
        print(f'Loading {filename} → {zone_name}')

        try:
            lon, lat, depth = load_grid(filepath)
        except Exception as e:
            print(f'  ERROR loading {filename}: {e}', file=sys.stderr)
            continue

        max_data_depth = float(np.nanmax(depth)) if not np.all(np.isnan(depth)) else 0.0

        zone_written = False
        for depth_km in depth_levels:
            if depth_km > max_data_depth + 25:
                continue

            polygons = footprint_contours(lon, lat, depth, depth_km)

            if not polygons:
                counters['skipped'] += 1
                continue

            depth_dir = f'{depth_km:03d}'
            out_path = os.path.join(args.output, depth_dir, f'{zone_name}.bna')
            write_bna(out_path, zone_name, depth_km, polygons)

            n_poly = len(polygons)
            n_pts = sum(len(p[0]) for p in polygons)
            print(f'  {depth_dir} km: {n_poly} polygon(s), {n_pts} pts → {out_path}')
            counters['written'] += 1
            zone_written = True

        if zone_written:
            written_zones.add(zone_name)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--input', required=True,
                   help='Directory with USGS Slab2.0 final output grids '
                        '(*_slab2_dep_*.grd, e.g. Slab2Distribute_Mar2018/)')
    p.add_argument('--input-guide',
                   help='Directory with guide-file .grd supplements for zones '
                        'absent from the final release (1car, exp, pan)')
    p.add_argument('--output', required=True,
                   help='Output root directory (depth-level subdirs created here)')
    p.add_argument('--min-depth', type=int, default=0,
                   help='Minimum depth level to generate in km (default: 0)')
    p.add_argument('--max-depth', type=int, default=700,
                   help='Maximum depth level to generate in km (default: 700)')
    p.add_argument('--zone', nargs='+',
                   help='Only process these zone output names (e.g. cascadia hellenic)')
    return p.parse_args()


def main():
    args = parse_args()

    depth_levels = [d for d in DEPTH_LEVELS_KM
                    if args.min_depth <= d <= args.max_depth]

    counters = {'written': 0, 'skipped': 0}
    written_zones = set()

    # Primary: final Slab2.0 output grids (27 zones)
    process_directory(args.input, ZONE_NAMES_SLAB2, zone_code_slab2,
                      args, depth_levels, written_zones, counters)

    # Supplement: guide files for zones not in the 2018 release (1car, exp, pan)
    if args.input_guide:
        print(f'\n--- Guide supplements from {args.input_guide} ---')
        process_directory(args.input_guide, ZONE_NAMES_GUIDE, zone_code_guide,
                          args, depth_levels, written_zones, counters)

    print(f'\nDone. {counters["written"]} BNA files written, '
          f'{counters["skipped"]} depth levels skipped (insufficient coverage).')


if __name__ == '__main__':
    main()
