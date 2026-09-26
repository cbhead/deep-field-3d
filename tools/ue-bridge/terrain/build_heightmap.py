#!/usr/bin/env python3
"""build_heightmap.py — render a text-authored landform (terrain.json) to a 16-bit heightmap.

The terrain lane of ADR-0018 / PROGRAMME.md §3.2: `unreal/content/terrain/<map>.terrain.json`
is the source of truth for a map's relief; this script renders it deterministically (same spec
+ same seed -> byte-identical outputs) and the `DFTerrainImport` commandlet turns the outputs
into the `L_<Map>_Terrain` Landscape. Nobody sculpts the Landscape by hand except through the
recorded polish layer `<map>_sculpt_delta.png`, which is re-applied here after regeneration.

    python3 tools/ue-bridge/terrain/build_heightmap.py foundry
    python3 tools/ue-bridge/terrain/build_heightmap.py foundry --out /tmp/x --no-sculpt-delta

Pure Python 3 (stdlib only: the dev Mac has neither numpy nor PIL); PNGs are written with zlib.

Coordinate frame — everything in the spec is SIM METRES:
    x  right   (+x = east on the legacy maps)
    z  forward is -z, so +z points toward the camera / the spawn yard on the legacy maps
    y  up (elevations, `baseElevation`, `height`, `depth`, `level`)
Sample (col, row) of every output image sits at sim (originX + col*res, originZ + row*res):
columns run along +x, rows along +z. The Unreal mapping (Unreal.X = -z*100, Y = x*100,
Z = y*100) is applied by the importer, never here; the images stay in the sim frame so the
same PNGs feed the validator, the lane projector and the Landscape import alike.

Outputs (in `unreal/content/terrain/out/`):
    <map>_height.png        16-bit grayscale; 0 = minZ, 65535 = maxZ (the range is in the json)
    <map>_height.json       {minZ, maxZ, metresPerSample, originX, originZ, width, height, ...}
    <map>_mask_steep.png    8-bit; slope-derived (0 at <=30 % grade, 255 at >=70 %)
    <map>_mask_<kind>.png   8-bit; the max blend weight of every feature of that kind
    <map>_mask_water.png    8-bit; where water[] regions are below their level (only if water[])
    <map>_preview.png       8-bit; hillshade + 2 m contour lines, for a quick look in a browser

How the height field is built (see unreal/content/schema/terrain.schema.json for every field):
    1. h = baseElevation everywhere.
    2. features[] and noise[] are applied in ascending `priority` (ties: features before noise,
       then array order). Features are distance-field blends along polyline splines with a
       named profile (smooth | sharp | stepped); additive kinds (ridge, embankment, valley, cut,
       saddle) add or subtract, region kinds (plateau, basin) blend toward a level, step kinds
       (terrace, cliff) raise one side of an edge, shore flattens one side to a bed level, and
       roadbed re-grades the ground under a lane to <= maxGrade % and flattens it laterally.
       A later (higher-priority) feature overrides an earlier one where they overlap.
    3. flats[] override everything: explicitly flat polygons at an elevation, with a reason.
    4. <map>_sculpt_delta.png (16-bit, 32768 = 0, one unit = 1 cm) is added if present.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
TERRAIN_DIR = REPO / "unreal" / "content" / "terrain"

FEATURE_KINDS = ("ridge", "valley", "plateau", "terrace", "cut", "embankment", "basin", "saddle",
                 "cliff", "roadbed", "shore")
ADDITIVE_RAISE = {"ridge", "embankment"}
ADDITIVE_LOWER = {"valley", "cut", "saddle"}
REGION_KINDS = {"plateau", "basin"}
STEP_KINDS = {"terrace", "cliff"}
SCULPT_DELTA_UNIT = 0.01      # metres per 16-bit unit in <map>_sculpt_delta.png (32768 = 0)
STEEP_LOW, STEEP_HIGH = 0.30, 0.70   # slope (rise/run) range mapped to 0..255 in _mask_steep


# ----------------------------------------------------------------------------------------------
# Small geometry helpers (no numpy on this machine; the grids are ~30k samples, this is fine)
# ----------------------------------------------------------------------------------------------

def clamp01(t: float) -> float:
    return 0.0 if t < 0.0 else 1.0 if t > 1.0 else t


def profile(kind: str, t: float, steps: int = 4) -> float:
    """Blend weight for a normalised distance t in [0,1] (1 = fully inside / on the high side)."""
    t = clamp01(t)
    if kind == "sharp":
        return t
    if kind == "stepped":
        n = max(2, int(steps))
        # n treads from 0 to 1: tread k sits at k/(n-1); the last tread is reached at t == 1.
        return min(n - 1, math.floor(t * n)) / (n - 1)
    # smooth (default): smoothstep — continuous slope at both ends so nothing reads as a crease.
    return t * t * (3.0 - 2.0 * t)


def seg_distance(px: float, pz: float, ax: float, az: float, bx: float, bz: float) -> tuple[float, float, float]:
    """Distance from P to segment AB, the parameter u in [0,1] of the closest point, and the
    signed cross product (positive = P is left of A->B, looking down +y)."""
    dx, dz = bx - ax, bz - az
    l2 = dx * dx + dz * dz
    if l2 <= 0.0:
        ex, ez = px - ax, pz - az
        return math.hypot(ex, ez), 0.0, 0.0
    u = ((px - ax) * dx + (pz - az) * dz) / l2
    u = clamp01(u)
    cx, cz = ax + u * dx, az + u * dz
    cross = dx * (pz - az) - dz * (px - ax)
    return math.hypot(px - cx, pz - cz), u, cross


def polyline_distance(px: float, pz: float, pts: list) -> tuple[float, int, float, float]:
    """Nearest (distance, segment index, u, signed side) of P to an open polyline."""
    if len(pts) == 1:
        return math.hypot(px - pts[0][0], pz - pts[0][1]), 0, 0.0, 0.0
    best = (float("inf"), 0, 0.0, 0.0)
    for i in range(len(pts) - 1):
        d, u, cross = seg_distance(px, pz, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])
        if d < best[0]:
            best = (d, i, u, cross)
    return best


def point_in_polygon(px: float, pz: float, poly: list) -> bool:
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, zi = poly[i][0], poly[i][1]
        xj, zj = poly[j][0], poly[j][1]
        if (zi > pz) != (zj > pz):
            x_at = xj + (pz - zj) * (xi - xj) / (zi - zj)
            if px < x_at:
                inside = not inside
        j = i
    return inside


def polygon_signed_distance(px: float, pz: float, poly: list) -> float:
    """Negative inside, positive outside; magnitude = distance to the boundary."""
    closed = list(poly) + [poly[0]]
    d = polyline_distance(px, pz, closed)[0]
    return -d if point_in_polygon(px, pz, poly) else d


def polyline_length(pts: list) -> float:
    return sum(math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]) for i in range(len(pts) - 1))


def resample_polyline(pts: list, spacing: float) -> list:
    """Points every <= spacing metres along the polyline, keeping every original vertex.
    Each entry is [x, z, y-or-None, arc-length]."""
    out = [[pts[0][0], pts[0][1], pts[0][2] if len(pts[0]) > 2 else None, 0.0]]
    s = 0.0
    for i in range(len(pts) - 1):
        ax, az = pts[i][0], pts[i][1]
        bx, bz = pts[i + 1][0], pts[i + 1][1]
        ay = pts[i][2] if len(pts[i]) > 2 else None
        by = pts[i + 1][2] if len(pts[i + 1]) > 2 else None
        length = math.hypot(bx - ax, bz - az)
        n = max(1, int(math.ceil(length / spacing)))
        for k in range(1, n + 1):
            u = k / n
            y = None
            if ay is not None and by is not None:
                y = ay + (by - ay) * u
            elif k == n:
                y = by
            out.append([ax + (bx - ax) * u, az + (bz - az) * u, y, s + length * u])
        s += length
    return out


# ----------------------------------------------------------------------------------------------
# Value noise with a seeded lattice (deterministic: integer hashing only)
# ----------------------------------------------------------------------------------------------

def lattice_value(ix: int, iy: int, seed: int) -> float:
    """Hash of a lattice corner -> [-1, 1]. Pure integer arithmetic so every platform agrees."""
    h = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF) / 0x7FFFFF - 1.0


def value_noise(x: float, y: float, seed: int) -> float:
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    # Quintic fade: zero first and second derivative at the corners (no lattice creases).
    ux = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
    uy = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)
    v00 = lattice_value(ix, iy, seed)
    v10 = lattice_value(ix + 1, iy, seed)
    v01 = lattice_value(ix, iy + 1, seed)
    v11 = lattice_value(ix + 1, iy + 1, seed)
    top = v00 + (v10 - v00) * ux
    bottom = v01 + (v11 - v01) * ux
    return top + (bottom - top) * uy


def fbm(x: float, z: float, octaves: int, frequency: float, seed: int, persistence: float, lacunarity: float) -> float:
    total, amp, freq = 0.0, 1.0, frequency
    for octave in range(max(1, octaves)):
        total += amp * value_noise(x * freq, z * freq, seed + octave * 7919)
        amp *= persistence
        freq *= lacunarity
    return total


# ----------------------------------------------------------------------------------------------
# The grid
# ----------------------------------------------------------------------------------------------

class Grid:
    """A row-major float field over the sim-frame sample lattice."""

    def __init__(self, spec: dict):
        bounds = spec["bounds"]
        playable = bounds["playable"]
        belt = float(bounds.get("belt", 0.0))
        centre = bounds.get("centre", [0.0, 0.0])
        self.res = float(spec.get("resolution", 1.0))
        extent_x = float(playable[0]) + 2.0 * belt
        extent_z = float(playable[1]) + 2.0 * belt
        self.width = int(math.ceil(extent_x / self.res)) + 1
        self.height = int(math.ceil(extent_z / self.res)) + 1
        self.origin_x = float(centre[0]) - extent_x / 2.0
        self.origin_z = float(centre[1]) - extent_z / 2.0
        self.playable = [float(playable[0]), float(playable[1])]
        self.belt = belt
        self.centre = [float(centre[0]), float(centre[1])]
        self.h = [float(spec.get("baseElevation", 0.0))] * (self.width * self.height)

    def coords(self, col: int, row: int) -> tuple[float, float]:
        return self.origin_x + col * self.res, self.origin_z + row * self.res

    def sample(self, x: float, z: float) -> float:
        """Bilinear sample of the current field (edge-clamped)."""
        fx = (x - self.origin_x) / self.res
        fz = (z - self.origin_z) / self.res
        fx = min(max(fx, 0.0), self.width - 1.0)
        fz = min(max(fz, 0.0), self.height - 1.0)
        c0, r0 = int(math.floor(fx)), int(math.floor(fz))
        c1, r1 = min(c0 + 1, self.width - 1), min(r0 + 1, self.height - 1)
        tx, tz = fx - c0, fz - r0
        h = self.h
        w = self.width
        top = h[r0 * w + c0] + (h[r0 * w + c1] - h[r0 * w + c0]) * tx
        bottom = h[r1 * w + c0] + (h[r1 * w + c1] - h[r1 * w + c0]) * tx
        return top + (bottom - top) * tz


# ----------------------------------------------------------------------------------------------
# Features
# ----------------------------------------------------------------------------------------------

def feature_region(feature: dict) -> tuple[list | None, tuple | None]:
    """A region feature is either a closed polygon (spline with >= 3 points) or centre+radius."""
    if "centre" in feature:
        return None, (float(feature["centre"][0]), float(feature["centre"][1]), float(feature.get("radius", feature.get("width", 10.0) / 2.0)))
    return feature["spline"], None


def region_signed_distance(px: float, pz: float, poly: list | None, circle: tuple | None) -> float:
    if circle is not None:
        return math.hypot(px - circle[0], pz - circle[1]) - circle[2]
    return polygon_signed_distance(px, pz, poly)


def apply_additive(grid: Grid, f: dict, sign: float, weights: list) -> None:
    pts = f["spline"]
    half_w = float(f.get("width", 0.0)) / 2.0
    falloff = float(f.get("falloff", f.get("width", 0.0)))
    reach = half_w + falloff
    amount = float(f.get("height", f.get("depth", 0.0)))
    prof = f.get("profile", "smooth")
    steps = int(f.get("steps", 4))
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            d = polyline_distance(px, pz, pts)[0]
            if d > reach:
                continue
            w = 1.0 if d <= half_w else (profile(prof, 1.0 - (d - half_w) / falloff, steps) if falloff > 0 else 0.0)
            if w <= 0.0:
                continue
            i = row * grid.width + col
            grid.h[i] += sign * amount * w
            if w > weights[i]:
                weights[i] = w


def apply_region(grid: Grid, f: dict, weights: list) -> None:
    poly, circle = feature_region(f)
    falloff = max(1e-6, float(f.get("falloff", 4.0)))
    prof = f.get("profile", "smooth")
    steps = int(f.get("steps", 4))
    kind = f["kind"]
    elevation = f.get("elevation")
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            sd = region_signed_distance(px, pz, poly, circle)
            if kind == "basin" and "elevation" not in f:
                # A bowl: full depth once `falloff` metres inside the rim, nothing outside it.
                if sd >= 0.0:
                    continue
                w = profile(prof, -sd / falloff, steps)
            else:
                if sd >= falloff:
                    continue
                w = 1.0 if sd <= 0.0 else profile(prof, 1.0 - sd / falloff, steps)
            if w <= 0.0:
                continue
            i = row * grid.width + col
            if elevation is not None:
                grid.h[i] += (float(elevation) - grid.h[i]) * w
            elif kind == "basin":
                grid.h[i] -= float(f.get("depth", 0.0)) * w
            else:
                grid.h[i] += float(f.get("height", 0.0)) * w
            if w > weights[i]:
                weights[i] = w


def apply_step(grid: Grid, f: dict, weights: list) -> None:
    """terrace / cliff: the ground on one side of the edge polyline is `height` higher; the face
    is `width` metres wide, centred on the line. `side` names the high side ("left" = left of
    the direction of travel, looking down +y)."""
    pts = f["spline"]
    width = max(1e-6, float(f.get("width", 4.0 if f["kind"] == "terrace" else 1.0)))
    height = float(f.get("height", 0.0))
    prof = f.get("profile", "smooth" if f["kind"] == "terrace" else "sharp")
    steps = int(f.get("steps", 4))
    high_left = f.get("side", "left") == "left"
    half = width / 2.0
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            d, _seg, _u, cross = polyline_distance(px, pz, pts)
            on_high = (cross > 0.0) == high_left
            if d >= half:
                w = 1.0 if on_high else 0.0
            else:
                s = d if on_high else -d
                w = profile(prof, (s + half) / width, steps)
            if w <= 0.0:
                continue
            i = row * grid.width + col
            grid.h[i] += height * w
            if w > weights[i]:
                weights[i] = w


def apply_shore(grid: Grid, f: dict, weights: list) -> None:
    """One-sided plateau: on the `side` (default right) of the line the ground is blended to
    `elevation` (the bed) over `falloff` metres, then flat at the bed."""
    pts = f["spline"]
    falloff = max(1e-6, float(f.get("falloff", f.get("width", 6.0))))
    elevation = float(f["elevation"])
    prof = f.get("profile", "smooth")
    steps = int(f.get("steps", 4))
    bed_left = f.get("side", "right") == "left"
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            d, _seg, _u, cross = polyline_distance(px, pz, pts)
            on_bed = (cross > 0.0) == bed_left
            if not on_bed:
                continue
            w = profile(prof, d / falloff, steps)
            if w <= 0.0:
                continue
            i = row * grid.width + col
            grid.h[i] += (elevation - grid.h[i]) * w
            if w > weights[i]:
                weights[i] = w


def grade_limit(stations: list, max_grade: float, mode: str) -> list:
    """Clamp a bed profile so |dh/ds| <= max_grade along the stations. "fill" only raises (the
    road rides over the drop on a built-up ramp that starts at the edge), "cut" only lowers (it
    digs in and reaches the low ground at the edge), and "balanced" (default) moves both ends of
    every too-steep pair toward each other by the same amount — cut volume equals fill volume
    and the ramp is centred on the edge it crosses, at the full grade on both sides."""
    n = len(stations)
    h = [s[2] for s in stations]
    lims = [max_grade * (stations[i][3] - stations[i - 1][3]) for i in range(1, n)]
    if mode in ("fill", "cut"):
        raise_only = mode == "fill"
        for _ in range(64):
            changed = False
            for i in range(1, n):
                lim = lims[i - 1]
                if raise_only and h[i] < h[i - 1] - lim:
                    h[i] = h[i - 1] - lim; changed = True
                elif not raise_only and h[i] > h[i - 1] + lim:
                    h[i] = h[i - 1] + lim; changed = True
            for i in range(n - 2, -1, -1):
                lim = lims[i]
                if raise_only and h[i] < h[i + 1] - lim:
                    h[i] = h[i + 1] - lim; changed = True
                elif not raise_only and h[i] > h[i + 1] + lim:
                    h[i] = h[i + 1] + lim; changed = True
            if not changed:
                break
        return h
    # Balanced: cyclic projection onto the convex sets {|h[i] - h[i-1]| <= lim}. Each projection
    # splits the excess equally, so the sum of heights never changes (cut == fill) and the
    # iteration converges to a profile inside every constraint. Sweep both ways for symmetry.
    for _ in range(20000):
        worst = 0.0
        for i in list(range(1, n)) + list(range(n - 1, 0, -1)):
            lim = lims[i - 1]
            d = h[i] - h[i - 1]
            excess = abs(d) - lim
            if excess > 1e-9:
                half = excess / 2.0
                if d > 0:
                    h[i] -= half; h[i - 1] += half
                else:
                    h[i] += half; h[i - 1] -= half
                worst = max(worst, excess)
        if worst <= 1e-7:
            break
    return h


def apply_roadbed(grid: Grid, f: dict, weights: list) -> dict:
    """Flatten the corridor under a lane and limit its grade. The bed elevation at each station
    comes from the vertex y (if the spline is 3D) or from the terrain as it stands, then the grade
    limiter re-profiles it. Returns the graded profile for the stats block."""
    pts = f["spline"]
    half_w = float(f.get("width", 5.0)) / 2.0
    falloff = float(f.get("falloff", 3.0))
    reach = half_w + falloff
    max_grade = float(f.get("maxGrade", 30.0)) / 100.0
    prof = f.get("profile", "smooth")
    steps = int(f.get("steps", 4))
    stations = resample_polyline(pts, min(1.0, grid.res))
    for st in stations:
        if st[2] is None:
            st[2] = grid.sample(st[0], st[1])
    bed = grade_limit(stations, max_grade, f.get("gradeMode", "balanced"))
    for st, b in zip(stations, bed):
        st[2] = b
    fine = [(s[0], s[1]) for s in stations]
    # Coarse reject on the authored polyline first: the fine polyline has ~1 segment per metre.
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            if polyline_distance(px, pz, pts)[0] > reach + grid.res:
                continue
            d, seg, u, _cross = polyline_distance(px, pz, fine)
            if d > reach:
                continue
            b = stations[seg][2] + (stations[min(seg + 1, len(stations) - 1)][2] - stations[seg][2]) * u
            w = 1.0 if d <= half_w else (profile(prof, 1.0 - (d - half_w) / falloff, steps) if falloff > 0 else 0.0)
            if w <= 0.0:
                continue
            i = row * grid.width + col
            grid.h[i] += (b - grid.h[i]) * w
            if w > weights[i]:
                weights[i] = w
    max_seen = 0.0
    for i in range(1, len(stations)):
        ds = stations[i][3] - stations[i - 1][3]
        if ds > 0:
            max_seen = max(max_seen, abs(stations[i][2] - stations[i - 1][2]) / ds)
    return {"id": f.get("id"), "lengthMetres": round(stations[-1][3], 3), "maxGradePercent": round(max_seen * 100.0, 2),
            "startY": round(stations[0][2], 3), "endY": round(stations[-1][2], 3)}


def apply_feature(grid: Grid, f: dict, weights: list, stats: dict) -> None:
    kind = f["kind"]
    if kind not in FEATURE_KINDS:
        raise ValueError(f"feature {f.get('id')!r}: unknown kind {kind!r}")
    if kind in ADDITIVE_RAISE:
        apply_additive(grid, f, +1.0, weights)
    elif kind in ADDITIVE_LOWER:
        apply_additive(grid, f, -1.0, weights)
    elif kind in REGION_KINDS:
        apply_region(grid, f, weights)
    elif kind in STEP_KINDS:
        apply_step(grid, f, weights)
    elif kind == "shore":
        apply_shore(grid, f, weights)
    elif kind == "roadbed":
        stats.setdefault("roadbeds", []).append(apply_roadbed(grid, f, weights))


# ----------------------------------------------------------------------------------------------
# Noise, flats, sculpt delta
# ----------------------------------------------------------------------------------------------

def mask_field(grid: Grid, mask, feature_weights: dict) -> list | None:
    """None means "everywhere". A string names a feature (its blend weight is the mask); an
    object is {feature} | {polygon, falloff?} | {region: "playable"|"belt"}, with `invert`."""
    if mask is None or mask == "all":
        return None
    if isinstance(mask, str):
        mask = {"feature": mask}
    invert = bool(mask.get("invert", False))
    n = grid.width * grid.height
    if "feature" in mask:
        if mask["feature"] not in feature_weights:
            raise ValueError(f"noise mask names unknown feature {mask['feature']!r} (features must come earlier in priority)")
        field = list(feature_weights[mask["feature"]])
    elif "polygon" in mask:
        falloff = max(1e-6, float(mask.get("falloff", 1e-6)))
        poly = mask["polygon"]
        field = [0.0] * n
        for row in range(grid.height):
            for col in range(grid.width):
                px, pz = grid.coords(col, row)
                sd = polygon_signed_distance(px, pz, poly)
                field[row * grid.width + col] = 1.0 if sd <= 0.0 else profile("smooth", 1.0 - sd / falloff)
    elif "region" in mask:
        field = [0.0] * n
        hx, hz = grid.playable[0] / 2.0, grid.playable[1] / 2.0
        falloff = max(1e-6, float(mask.get("falloff", 4.0)))
        for row in range(grid.height):
            for col in range(grid.width):
                px, pz = grid.coords(col, row)
                # Signed distance to the playable rectangle (negative inside).
                dx = abs(px - grid.centre[0]) - hx
                dz = abs(pz - grid.centre[1]) - hz
                outside = math.hypot(max(dx, 0.0), max(dz, 0.0))
                sd = outside if outside > 0.0 else max(dx, dz)
                inside_w = 1.0 if sd <= 0.0 else profile("smooth", 1.0 - sd / falloff)
                field[row * grid.width + col] = inside_w if mask["region"] == "playable" else 1.0 - inside_w
    else:
        raise ValueError(f"unsupported noise mask {mask!r}")
    if invert:
        field = [1.0 - v for v in field]
    return field


def apply_noise(grid: Grid, layer: dict, feature_weights: dict, base_seed: int) -> None:
    octaves = int(layer.get("octaves", 3))
    amplitude = float(layer.get("amplitude", 0.5))
    frequency = float(layer.get("frequency", 0.05))
    persistence = float(layer.get("persistence", 0.5))
    lacunarity = float(layer.get("lacunarity", 2.0))
    seed = int(layer.get("seed", base_seed))
    mask = mask_field(grid, layer.get("mask"), feature_weights)
    for row in range(grid.height):
        for col in range(grid.width):
            i = row * grid.width + col
            m = 1.0 if mask is None else mask[i]
            if m <= 0.0:
                continue
            px, pz = grid.coords(col, row)
            grid.h[i] += amplitude * m * fbm(px, pz, octaves, frequency, seed, persistence, lacunarity)


def apply_flat(grid: Grid, flat: dict, weights: list) -> None:
    poly = flat["polygon"]
    elevation = float(flat["elevation"])
    falloff = float(flat.get("falloff", 0.0))
    for row in range(grid.height):
        for col in range(grid.width):
            px, pz = grid.coords(col, row)
            sd = polygon_signed_distance(px, pz, poly)
            if sd <= 0.0:
                w = 1.0
            elif falloff > 0.0 and sd < falloff:
                w = profile("smooth", 1.0 - sd / falloff)
            else:
                continue
            i = row * grid.width + col
            grid.h[i] += (elevation - grid.h[i]) * w
            if w > weights[i]:
                weights[i] = w


def apply_sculpt_delta(grid: Grid, path: Path) -> bool:
    if not path.exists():
        return False
    width, height, depth, samples = read_png_gray(path.read_bytes())
    if depth != 16:
        raise ValueError(f"{path.name}: sculpt delta must be 16-bit grayscale")
    if (width, height) != (grid.width, grid.height):
        raise ValueError(f"{path.name}: {width}x{height} does not match the {grid.width}x{grid.height} grid — re-record it")
    for i, v in enumerate(samples):
        grid.h[i] += (v - 32768) * SCULPT_DELTA_UNIT
    return True


# ----------------------------------------------------------------------------------------------
# PNG in / out (stdlib only)
# ----------------------------------------------------------------------------------------------

def _chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png_gray(width: int, height: int, depth: int, samples: list) -> bytes:
    """Grayscale PNG, 8 or 16 bit, one filter byte (0) per row; zlib level 9 for stable bytes."""
    assert depth in (8, 16)
    fmt = ">%dH" % width if depth == 16 else "%dB" % width
    raw = bytearray()
    for row in range(height):
        raw.append(0)
        raw += struct.pack(fmt, *samples[row * width:(row + 1) * width])
    ihdr = struct.pack(">IIBBBBB", width, height, depth, 0, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + _chunk(b"IEND", b"")


def read_png_gray(data: bytes) -> tuple[int, int, int, list]:
    """Minimal reader for the PNGs this tool (or the editor's heightmap export) writes: grayscale,
    8/16-bit, non-interlaced, any of the five filter types."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, width, height, depth, ctype, idat = 8, 0, 0, 0, 0, bytearray()
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, ctype, _c, _f, interlace = struct.unpack(">IIBBBBB", body)
            if ctype != 0 or interlace != 0:
                raise ValueError("only non-interlaced grayscale PNGs are supported")
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
        pos += 12 + length
    raw = zlib.decompress(bytes(idat))
    bpp = 2 if depth == 16 else 1
    stride = width * bpp
    out: list = []
    prev = bytearray(stride)
    p = 0
    for _row in range(height):
        ftype = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        if depth == 16:
            out.extend(struct.unpack(">%dH" % width, bytes(line)))
        else:
            out.extend(line)
        prev = line
    return width, height, depth, out


# ----------------------------------------------------------------------------------------------
# Derived images
# ----------------------------------------------------------------------------------------------

def slope_field(grid: Grid) -> list:
    """Rise/run per sample from central differences (one-sided at the border)."""
    w, hgt, h, res = grid.width, grid.height, grid.h, grid.res
    out = [0.0] * (w * hgt)
    for row in range(hgt):
        for col in range(w):
            c0, c1 = max(col - 1, 0), min(col + 1, w - 1)
            r0, r1 = max(row - 1, 0), min(row + 1, hgt - 1)
            dx = (h[row * w + c1] - h[row * w + c0]) / ((c1 - c0) * res)
            dz = (h[r1 * w + col] - h[r0 * w + col]) / ((r1 - r0) * res)
            out[row * w + col] = math.hypot(dx, dz)
    return out


def to_bytes8(field: list) -> list:
    return [int(round(clamp01(v) * 255.0)) for v in field]


def preview_field(grid: Grid) -> list:
    """Hillshade (light from the north-west, i.e. -x -z) with a contour line every 2 m."""
    w, hgt, h, res = grid.width, grid.height, grid.h, grid.res
    lx, ly, lz = -0.5, 0.7, -0.5
    norm = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / norm, ly / norm, lz / norm
    out = [0] * (w * hgt)
    for row in range(hgt):
        for col in range(w):
            c0, c1 = max(col - 1, 0), min(col + 1, w - 1)
            r0, r1 = max(row - 1, 0), min(row + 1, hgt - 1)
            dx = (h[row * w + c1] - h[row * w + c0]) / ((c1 - c0) * res)
            dz = (h[r1 * w + col] - h[r0 * w + col]) / ((r1 - r0) * res)
            nx, ny, nz = -dx, 1.0, -dz
            nn = math.sqrt(nx * nx + ny * ny + nz * nz)
            shade = max(0.0, (nx * lx + ny * ly + nz * lz) / nn)
            v = 40 + shade * 200
            level = math.floor(h[row * w + col] / 2.0)
            if any(math.floor(h[r * w + c] / 2.0) != level for r, c in ((row, c1), (r1, col))):
                v *= 0.55
            out[row * w + col] = int(round(min(255.0, v)))
    return out


# ----------------------------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------------------------

class BuildResult:
    def __init__(self, grid: Grid, masks: dict, meta: dict, preview: list):
        self.grid = grid
        self.masks = masks          # name -> 8-bit sample list
        self.meta = meta
        self.preview = preview

    def height16(self) -> list:
        lo, hi = self.meta["minZ"], self.meta["maxZ"]
        span = hi - lo
        if span <= 0.0:
            return [0] * len(self.grid.h)
        return [int(round((v - lo) / span * 65535.0)) for v in self.grid.h]

    def files(self, map_id: str) -> dict:
        """Output file name -> bytes. Deterministic: no timestamps anywhere."""
        out = {
            f"{map_id}_height.png": write_png_gray(self.grid.width, self.grid.height, 16, self.height16()),
            f"{map_id}_height.json": (json.dumps(self.meta, indent=2, sort_keys=True) + "\n").encode("utf-8"),
            f"{map_id}_preview.png": write_png_gray(self.grid.width, self.grid.height, 8, self.preview),
        }
        for name, samples in self.masks.items():
            out[f"{map_id}_mask_{name}.png"] = write_png_gray(self.grid.width, self.grid.height, 8, samples)
        return out


def build(spec: dict, sculpt_delta: Path | None = None, source_bytes: bytes | None = None) -> BuildResult:
    grid = Grid(spec)
    n = grid.width * grid.height
    seed = int(spec.get("seed", 0))
    stats: dict = {}
    feature_weights: dict = {}
    kind_weights: dict = {}

    ops = []
    for idx, f in enumerate(spec.get("features", [])):
        ops.append((float(f.get("priority", 0)), 0, idx, "feature", f))
    for idx, layer in enumerate(spec.get("noise", [])):
        ops.append((float(layer.get("priority", 50)), 1, idx, "noise", layer))
    ops.sort(key=lambda op: (op[0], op[1], op[2]))

    for _prio, _tie, idx, what, item in ops:
        if what == "feature":
            weights = [0.0] * n
            apply_feature(grid, item, weights, stats)
            fid = item.get("id", f"{item['kind']}_{idx}")
            feature_weights[fid] = weights
            kw = kind_weights.setdefault(item["kind"], [0.0] * n)
            for i in range(n):
                if weights[i] > kw[i]:
                    kw[i] = weights[i]
        else:
            apply_noise(grid, item, feature_weights, seed)

    flat_weights = [0.0] * n
    for flat in spec.get("flats", []):
        apply_flat(grid, flat, flat_weights)

    delta_applied = False
    if sculpt_delta is not None:
        delta_applied = apply_sculpt_delta(grid, sculpt_delta)

    masks = {kind: to_bytes8(w) for kind, w in kind_weights.items()}
    if spec.get("flats"):
        masks["flat"] = to_bytes8(flat_weights)
    slope = slope_field(grid)
    masks["steep"] = to_bytes8([(s - STEEP_LOW) / (STEEP_HIGH - STEEP_LOW) for s in slope])

    water = spec.get("water", [])
    if water:
        wm = [0.0] * n
        for body in water:
            level = float(body["level"])
            if "spline" in body and body.get("kind") in ("river", "canal"):
                pts, half_w = body["spline"], float(body.get("width", 6.0)) / 2.0
                for row in range(grid.height):
                    for col in range(grid.width):
                        i = row * grid.width + col
                        if grid.h[i] >= level:
                            continue
                        px, pz = grid.coords(col, row)
                        if polyline_distance(px, pz, pts)[0] <= half_w:
                            wm[i] = 1.0
            else:
                poly, circle = feature_region(body)
                for row in range(grid.height):
                    for col in range(grid.width):
                        i = row * grid.width + col
                        if grid.h[i] >= level:
                            continue
                        px, pz = grid.coords(col, row)
                        if region_signed_distance(px, pz, poly, circle) <= 0.0:
                            wm[i] = 1.0
        masks["water"] = to_bytes8(wm)

    min_z, max_z = min(grid.h), max(grid.h)
    if max_z - min_z < 1e-6:
        max_z = min_z + 1.0   # a perfectly flat map still needs a non-degenerate range
    meta = {
        "map": spec.get("id"),
        "frame": "sim-metres",
        "axes": "column = +x (right), row = +z; sample (col,row) at (originX + col*metresPerSample, originZ + row*metresPerSample)",
        "unrealMapping": "Unreal.X = -z*100, Unreal.Y = x*100, Unreal.Z = y*100 (cm); applied by DFTerrainImport, not here",
        "minZ": round(min_z, 4),
        "maxZ": round(max_z, 4),
        "metresPerSample": grid.res,
        "originX": grid.origin_x,
        "originZ": grid.origin_z,
        "width": grid.width,
        "height": grid.height,
        "playable": grid.playable,
        "belt": grid.belt,
        "centre": grid.centre,
        "baseElevation": float(spec.get("baseElevation", 0.0)),
        "seed": seed,
        "heightEncoding": "16-bit: value = round((y - minZ) / (maxZ - minZ) * 65535)",
        "masks": sorted(masks.keys()),
        "water": [{"kind": b.get("kind"), "id": b.get("id"), "level": b["level"]} for b in water],
        "sculptDeltaApplied": delta_applied,
        "sculptDeltaUnitMetres": SCULPT_DELTA_UNIT,
        "stats": stats,
    }
    if source_bytes is not None:
        meta["sourceSha256"] = hashlib.sha256(source_bytes).hexdigest()
    return BuildResult(grid, masks, meta, preview_field(grid))


def build_map(map_id: str, terrain_dir: Path, out_dir: Path, use_sculpt_delta: bool = True) -> dict:
    src = terrain_dir / f"{map_id}.terrain.json"
    if not src.exists():
        raise SystemExit(f"no such terrain spec: {src}")
    source_bytes = src.read_bytes()
    spec = json.loads(source_bytes)
    delta = terrain_dir / f"{map_id}_sculpt_delta.png" if use_sculpt_delta else None
    result = build(spec, delta, source_bytes)
    out_dir.mkdir(parents=True, exist_ok=True)
    files = result.files(map_id)
    for name, data in files.items():
        (out_dir / name).write_bytes(data)
    return {"files": sorted(files.keys()), "meta": result.meta}


def main(argv: list | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("map", help="map id, e.g. foundry (reads <terrain-dir>/<map>.terrain.json)")
    ap.add_argument("--terrain-dir", type=Path, default=TERRAIN_DIR)
    ap.add_argument("--out", type=Path, default=None, help="output directory (default <terrain-dir>/out)")
    ap.add_argument("--no-sculpt-delta", action="store_true", help="ignore <map>_sculpt_delta.png even if present")
    args = ap.parse_args(argv)
    out_dir = args.out or (args.terrain_dir / "out")
    report = build_map(args.map, args.terrain_dir, out_dir, not args.no_sculpt_delta)
    meta = report["meta"]
    print(f"{args.map}: {meta['width']}x{meta['height']} @ {meta['metresPerSample']} m, y in [{meta['minZ']}, {meta['maxZ']}] m, "
          f"origin ({meta['originX']}, {meta['originZ']}), sculpt delta {'applied' if meta['sculptDeltaApplied'] else 'none'}")
    for rb in meta["stats"].get("roadbeds", []):
        print(f"  roadbed {rb['id']}: {rb['lengthMetres']} m, max grade {rb['maxGradePercent']} %, y {rb['startY']} -> {rb['endY']}")
    for name in report["files"]:
        print(f"  wrote {out_dir / name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
