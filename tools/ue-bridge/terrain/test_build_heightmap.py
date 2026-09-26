#!/usr/bin/env python3
"""Tests for build_heightmap.py — plain assertions, no pytest (the dev Mac has no extra packages).

    python3 tools/ue-bridge/terrain/test_build_heightmap.py

Covers: determinism (two builds -> identical bytes), the stepped terrace profile at known sample
points, flats being flat, output dimensions/metadata, the roadbed grade limiter, the sculpt-delta
re-apply, and the PNG writer/reader round trip.
"""
from __future__ import annotations

import json
import math
import struct
import sys
import tempfile
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_heightmap as bh  # noqa: E402


def spec_base(**overrides) -> dict:
    spec = {
        "$schema": "deepfield-terrain/1",
        "id": "test",
        "bounds": {"playable": [40, 20], "belt": 0},
        "resolution": 0.5,
        "baseElevation": 10.0,
        "seed": 1,
    }
    spec.update(overrides)
    return spec


def sample_at(result: bh.BuildResult, x: float, z: float) -> float:
    g = result.grid
    col = round((x - g.origin_x) / g.res)
    row = round((z - g.origin_z) / g.res)
    assert abs(g.origin_x + col * g.res - x) < 1e-9 and abs(g.origin_z + row * g.res - z) < 1e-9, "not a lattice point"
    return g.h[row * g.width + col]


def png_header(data: bytes) -> tuple[int, int, int]:
    width, height, depth = struct.unpack(">IIB", data[16:25])
    return width, height, depth


# ----------------------------------------------------------------------------------------------

def test_determinism():
    spec = spec_base(
        features=[
            {"id": "r", "kind": "ridge", "spline": [[-10, -5], [10, 5]], "height": 3, "width": 2, "falloff": 4},
            {"id": "bed", "kind": "roadbed", "spline": [[-18, 0], [18, 0]], "width": 3, "falloff": 2, "priority": 10},
        ],
        noise=[{"octaves": 3, "amplitude": 0.4, "frequency": 0.1, "mask": {"feature": "bed", "invert": True}}],
        flats=[{"polygon": [[12, 4], [18, 4], [18, 9], [12, 9]], "elevation": 11, "reason": "test"}],
    )
    src = json.dumps(spec).encode()
    a = bh.build(json.loads(src), None, src).files("test")
    b = bh.build(json.loads(src), None, src).files("test")
    assert a.keys() == b.keys()
    for name in a:
        assert a[name] == b[name], f"{name} differs between two builds"
    assert len(a["test_height.png"]) > 100


def test_terrace_stepped_profile():
    # A step along x=0 travelling +z: 'left' = -x = the high side. Face 6 m wide, 3 treads.
    spec = spec_base(features=[{
        "kind": "terrace", "spline": [[0, -20], [0, 20]], "height": 6, "width": 6,
        "profile": "stepped", "steps": 3, "side": "left",
    }])
    r = bh.build(spec)
    base = 10.0
    assert abs(sample_at(r, -10, 0) - (base + 6)) < 1e-9, "high side is +height"
    assert abs(sample_at(r, 10, 0) - base) < 1e-9, "low side is untouched"
    assert abs(sample_at(r, 0, 0) - (base + 3)) < 1e-9, "on the line: t=0.5 -> tread 1 of 3 = half"
    assert abs(sample_at(r, -1.5, 0) - (base + 6)) < 1e-9, "t=0.75 -> top tread"
    assert abs(sample_at(r, 1.5, 0) - base) < 1e-9, "t=0.25 -> bottom tread"
    assert abs(sample_at(r, -0.5, 4) - (base + 3)) < 1e-9, "t=0.583 -> still the middle tread"
    # The terrace mask carries the blend weight: 255 on the high side, 0 on the low side.
    g = r.grid
    hi = round((-10 - g.origin_x) / g.res) + round((0 - g.origin_z) / g.res) * g.width
    lo = round((10 - g.origin_x) / g.res) + round((0 - g.origin_z) / g.res) * g.width
    assert r.masks["terrace"][hi] == 255 and r.masks["terrace"][lo] == 0
    # 'right' flips the high side.
    spec["features"][0]["side"] = "right"
    r2 = bh.build(spec)
    assert abs(sample_at(r2, 10, 0) - (base + 6)) < 1e-9 and abs(sample_at(r2, -10, 0) - base) < 1e-9


def test_flats_are_flat():
    spec = spec_base(
        features=[{"kind": "ridge", "spline": [[-20, 0], [20, 0]], "height": 5, "width": 4, "falloff": 6}],
        noise=[{"octaves": 3, "amplitude": 1.0, "frequency": 0.2}],
        flats=[{"polygon": [[-5, -4], [5, -4], [5, 4], [-5, 4]], "elevation": 12.25, "reason": "pad"}],
    )
    r = bh.build(spec)
    g = r.grid
    inside, outside_changed = 0, 0
    for row in range(g.height):
        for col in range(g.width):
            x, z = g.coords(col, row)
            v = g.h[row * g.width + col]
            if -5 <= x <= 5 and -4 <= z <= 4:
                assert abs(v - 12.25) < 1e-9, f"flat sample at ({x},{z}) = {v}"
                inside += 1
            elif abs(v - 10.0) > 1e-6:
                outside_changed += 1
    assert inside == 21 * 17
    assert outside_changed > 0, "the ridge + noise must have shaped the ground outside the flat"
    h16 = r.height16()
    vals = {h16[row * g.width + col] for row in range(g.height) for col in range(g.width)
            if -5 <= g.coords(col, row)[0] <= 5 and -4 <= g.coords(col, row)[1] <= 4}
    assert len(vals) == 1, "a flat is one 16-bit value"
    assert "flat" in r.masks


def test_dimensions_and_metadata():
    spec = {"$schema": "deepfield-terrain/1", "id": "dims", "bounds": {"playable": [110, 80], "belt": 40},
            "resolution": 1.0, "baseElevation": 4.0, "seed": 9,
            "features": [{"kind": "ridge", "spline": [[0, 0], [10, 0]], "height": 12, "width": 4, "falloff": 6}]}
    src = json.dumps(spec).encode()
    r = bh.build(spec, None, src)
    files = r.files("dims")
    m = r.meta
    assert (m["width"], m["height"]) == (191, 161), (m["width"], m["height"])
    assert m["originX"] == -95.0 and m["originZ"] == -80.0
    assert m["metresPerSample"] == 1.0 and m["frame"] == "sim-metres"
    assert abs(m["minZ"] - 4.0) < 1e-6 and abs(m["maxZ"] - 16.0) < 1e-3
    assert png_header(files["dims_height.png"]) == (191, 161, 16)
    assert png_header(files["dims_mask_steep.png"]) == (191, 161, 8)
    assert png_header(files["dims_mask_ridge.png"]) == (191, 161, 8)
    assert png_header(files["dims_preview.png"]) == (191, 161, 8)
    meta = json.loads(files["dims_height.json"])
    assert meta["width"] == 191 and meta["sourceSha256"] and meta["sculptDeltaApplied"] is False
    w, h, depth, samples = bh.read_png_gray(files["dims_height.png"])
    assert (w, h, depth) == (191, 161, 16)
    assert min(samples) == 0 and max(samples) == 65535, "0 = minZ, 65535 = maxZ"
    # A sample at 1 m/sample lands on integer sim coordinates: (0,0) is the ridge crest.
    col, row = int(0 - m["originX"]), int(0 - m["originZ"])
    assert samples[row * w + col] == 65535
    # Half-metre resolution changes the sample count, not the extent.
    spec["resolution"] = 0.5
    r2 = bh.build(spec)
    assert (r2.grid.width, r2.grid.height) == (381, 321)


def test_roadbed_grade_limit():
    # An 8 m terrace crossed by a lane: the bed must be re-graded to <= 30 %.
    spec = spec_base(
        bounds={"playable": [80, 20], "belt": 0}, resolution=1.0,
        features=[
            {"kind": "terrace", "spline": [[0, -20], [0, 20]], "height": 8, "width": 4, "side": "left"},
            {"id": "bed", "kind": "roadbed", "spline": [[-38, 0], [38, 0]], "width": 4, "falloff": 2, "maxGrade": 30, "priority": 10},
        ],
    )
    r = bh.build(spec)
    rb = r.meta["stats"]["roadbeds"][0]
    assert rb["maxGradePercent"] <= 30.0 + 1e-6, rb
    assert abs(rb["startY"] - 18.0) < 1e-6 and abs(rb["endY"] - 10.0) < 1e-6
    prev = None
    for x in range(-38, 39):
        v = sample_at(r, x, 0)
        if prev is not None:
            assert abs(v - prev) <= 0.30 + 1e-6, f"grade {abs(v - prev) * 100:.1f} % at x={x}"
        prev = v
    # Balanced mode cuts as much as it fills: the bed crosses the edge at mid-height and the ramp
    # is at the full 30 % on both sides — 8 m / 0.3 = 26.7 m long, centred on x=0.
    assert abs(sample_at(r, 0, 0) - 14.0) < 0.3
    assert abs(sample_at(r, -15, 0) - 18.0) < 0.3 and abs(sample_at(r, 15, 0) - 10.0) < 0.3
    assert abs(sample_at(r, -6, 0) - 15.8) < 0.5 and abs(sample_at(r, 6, 0) - 12.2) < 0.5
    bed = r.meta["stats"]["roadbeds"][0]
    assert abs(bed["maxGradePercent"] - 30.0) < 0.5, bed
    # Laterally flat across the bed's width, so the corridor has no cross-slope.
    assert abs(sample_at(r, 5, 1) - sample_at(r, 5, -1)) < 1e-9
    # Fill-only never digs below the terrain; cut-only never rises above it.
    spec["features"][1]["gradeMode"] = "fill"
    assert abs(sample_at(bh.build(spec), -10, 0) - 18.0) < 1e-6
    spec["features"][1]["gradeMode"] = "cut"
    assert abs(sample_at(bh.build(spec), 10, 0) - 10.0) < 1e-6


def test_sculpt_delta_and_cli():
    with tempfile.TemporaryDirectory() as tmp:
        tdir = Path(tmp) / "terrain"
        tdir.mkdir()
        spec = spec_base(id="delta", features=[{"kind": "ridge", "spline": [[-5, 0], [5, 0]], "height": 2, "width": 2, "falloff": 3}])
        (tdir / "delta.terrain.json").write_text(json.dumps(spec))
        first = bh.build_map("delta", tdir, tdir / "out")
        assert first["meta"]["sculptDeltaApplied"] is False
        base = bh.build(spec)
        # Record a +1 m polish at one sample and a -0.5 m at another (32768 = 0, 1 unit = 1 cm).
        n = base.grid.width * base.grid.height
        delta = [32768] * n
        i_up = 7 * base.grid.width + 11
        i_down = 20 * base.grid.width + 40
        delta[i_up] = 32768 + 100
        delta[i_down] = 32768 - 50
        (tdir / "delta_sculpt_delta.png").write_bytes(bh.write_png_gray(base.grid.width, base.grid.height, 16, delta))
        second = bh.build_map("delta", tdir, tdir / "out")
        assert second["meta"]["sculptDeltaApplied"] is True
        polished = bh.build(spec, tdir / "delta_sculpt_delta.png")
        assert abs(polished.grid.h[i_up] - base.grid.h[i_up] - 1.0) < 1e-9
        assert abs(polished.grid.h[i_down] - base.grid.h[i_down] + 0.5) < 1e-9
        assert sum(1 for a, b in zip(polished.grid.h, base.grid.h) if abs(a - b) > 1e-12) == 2
        # The CLI wrote every file the build promises.
        for name in first["files"]:
            assert (tdir / "out" / name).exists(), name
        # --no-sculpt-delta ignores the recorded layer.
        assert bh.build_map("delta", tdir, tdir / "out", use_sculpt_delta=False)["meta"]["sculptDeltaApplied"] is False


def test_png_round_trip():
    w, h = 13, 7
    samples16 = [(i * 5039) % 65536 for i in range(w * h)]
    data = bh.write_png_gray(w, h, 16, samples16)
    assert bh.read_png_gray(data) == (w, h, 16, samples16)
    samples8 = [(i * 37) % 256 for i in range(w * h)]
    data8 = bh.write_png_gray(w, h, 8, samples8)
    assert bh.read_png_gray(data8) == (w, h, 8, samples8)


def test_noise_mask_needs_earlier_feature():
    spec = spec_base(features=[{"id": "late", "kind": "ridge", "spline": [[0, 0], [1, 0]], "height": 1, "width": 1, "priority": 90}],
                     noise=[{"amplitude": 1, "mask": "late"}])
    try:
        bh.build(spec)
    except ValueError as e:
        assert "late" in str(e)
    else:
        raise AssertionError("a noise mask naming a later feature must be rejected")


def test_profiles():
    assert bh.profile("sharp", 0.25) == 0.25
    assert abs(bh.profile("smooth", 0.5) - 0.5) < 1e-12 and bh.profile("smooth", 0.0) == 0.0 and bh.profile("smooth", 1.0) == 1.0
    assert [bh.profile("stepped", t, 4) for t in (0.0, 0.24, 0.26, 0.5, 0.76, 1.0)] == [0.0, 0.0, 1 / 3, 2 / 3, 1.0, 1.0]
    assert math.isclose(bh.polygon_signed_distance(0, 0, [[-1, -1], [1, -1], [1, 1], [-1, 1]]), -1.0)
    assert math.isclose(bh.polygon_signed_distance(3, 0, [[-1, -1], [1, -1], [1, 1], [-1, 1]]), 2.0)


def main() -> int:
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"ok   {t.__name__}")
        except Exception:
            failed += 1
            print(f"FAIL {t.__name__}")
            traceback.print_exc()
    print(f"{len(tests) - failed}/{len(tests)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
