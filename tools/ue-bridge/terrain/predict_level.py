#!/usr/bin/env python3
"""predict_level.py — what DF.Map.Validate should say about a level on its terrain, before an engine runs it.

A map redesigned for terrain (ADR-0018) is JSON: `unreal/content/terrain/<map>.terrain.json` and
`unreal/content/levels/<map>.level.json`. The real measurement is the engine's (`-run=DFLevelImport`,
then `-run=DFMapValidate`, whose traces hit the Landscape *and* the static world). This script is the
closest a machine without the engine can get: it renders the terrain with build_heightmap.build() in
memory, projects the level onto it the way DFLevelImport does, and re-derives the rules that are pure
geometry, so a layout can be iterated against terrain instead of against taste.

    python3 tools/ue-bridge/terrain/predict_level.py foundry
    python3 tools/ue-bridge/terrain/predict_level.py foundry --png /tmp/foundry.png   # + an overlay, 4 px per metre
    python3 tools/ue-bridge/terrain/predict_level.py foundry --suggest 10              # + where a new pad would fix dead ground

Writes `unreal/content/levels/reports/<map>.predicted.json` (deterministic: same inputs, same bytes) and
prints a summary. Exit 1 if a rule the engine enforces is predicted to fail.

What it models (numbers from DFMapValidateCommandlet.cpp and map-authoring-3d.md):
  - the lane graph, approximately: routes split at the waypoints where they meet or part; shared spans
    are one edge (DFLaneGraphBuilder coalesces the same way; its near-miss tolerance is not modelled)
  - ground points projected to the terrain; air points at terrain + the authored AGL, interpolated
  - socketOffset (tower pads >= 3.5 m, 3D, from every walk edge), spawnApron (every socket >= 8 m, 3D,
    from every spawn node), coverage (a segment every 4 m of every walk edge, >= 3 tower pads within
    16 m / 15 m air, 3D, with a clear sight line from pad + 1.6 m to segment + 1.6 m) and routeIds
  - rules the engine does not implement yet, reported for design: lane grade (<= 30 %, 1 m samples),
    the slope band (> +15 % slows, < -15 % speeds), the boss route (<= 15 %, no warps), socket ground
    slope before the pad cut (<= 25 %), air AGL (+/- 2 m of authored, >= 3 m clearance), relief along
    the lane (G2: >= 8 m), and rule 16's terrain proof: a pad whose Lance is blocked by the ground and
    whose Nova arc clears it
What it cannot model: anything that is not terrain (decks, dressing, the pad cut itself, the navmesh),
so a wall socket's deck is assumed to exist at the socket's authored height and nothing else occludes.
Nova's projectile gravity is not in towers.json yet: 9.81 m/s^2 is assumed (the report says so).
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

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build_heightmap as bh  # noqa: E402

REPO = HERE.parents[2]
CONTENT = REPO / "unreal" / "content"

SEGMENT_M = 4.0
GROUND_RANGE_M = 16.0
AIR_RANGE_M = 15.0
EYE_M = 1.6
COVERAGE_REQUIRED = 3
SPAWN_APRON_M = 8.0
SOCKET_OFF_LANE_M = 3.5
LANE_MAX_GRADE = 30.0
SLOPE_BAND = 15.0
BOSS_MAX_GRADE = 15.0
PAD_UNGRADED_MAX = 25.0
AIR_AGL = (8.0, 15.0)
AIR_AGL_TOLERANCE_M = 2.0
AIR_CLEARANCE_M = 3.0
RELIEF_REQUIRED_M = 8.0
GRAVITY = 9.81            # assumed: towers.json has no projectileGravity for nova yet
TOWERS = {"lance": {"range": 12.0, "pitch": (-8.0, 26.0)}, "nova": {"range": 16.0, "min": 5.0, "pitch": (22.0, 70.0), "speed": 14.0}}


def r2(v: float) -> float:
    return round(v, 2)


# ---- terrain -------------------------------------------------------------------------------

class Terrain:
    def __init__(self, spec: dict):
        self.grid = bh.build(spec).grid

    def h(self, x: float, z: float) -> float:
        return self.grid.sample(x, z)

    def slope_percent(self, x: float, z: float, radius: float = 1.5) -> float:
        """Steepest grade across a pad-sized neighbourhood (the ground before the pad is cut)."""
        worst = 0.0
        for k in range(8):
            a = k * math.pi / 8.0
            dx, dz = math.cos(a) * radius, math.sin(a) * radius
            worst = max(worst, abs(self.h(x + dx, z + dz) - self.h(x - dx, z - dz)) / (2 * radius) * 100.0)
        return worst

    def clear(self, a: tuple, b: tuple, step: float = 0.25, skip: float = 0.3) -> bool:
        """True when the straight segment a->b (sim x, y, z) stays above the terrain."""
        n = max(2, int(math.dist(a, b) / step))
        for i in range(n + 1):
            t = i / n
            d = t * math.dist(a, b)
            if d < skip or math.dist(a, b) - d < skip:
                continue
            x, y, z = (a[j] + (b[j] - a[j]) * t for j in range(3))
            if self.h(x, z) > y + 0.02:
                return False
        return True


def arc_clear(terrain: Terrain, a: tuple, b: tuple, speed: float, pitch: tuple) -> float | None:
    """Launch pitch (degrees) of a ballistic arc a->b that clears the terrain, lob first; None if none."""
    dx = math.hypot(b[0] - a[0], b[2] - a[2])
    dy = b[1] - a[1]
    if dx < 1e-3:
        return None
    v2 = speed * speed
    disc = v2 * v2 - GRAVITY * (GRAVITY * dx * dx + 2 * dy * v2)
    if disc < 0:
        return None
    for sign in (1.0, -1.0):
        theta = math.atan((v2 + sign * math.sqrt(disc)) / (GRAVITY * dx))
        deg = math.degrees(theta)
        if not (pitch[0] <= deg <= pitch[1]):
            continue
        vx, vy = speed * math.cos(theta), speed * math.sin(theta)
        t_end = dx / vx
        ok = True
        steps = max(8, int(dx / 0.25))
        for i in range(1, steps):
            t = t_end * i / steps
            f = i / steps
            x = a[0] + (b[0] - a[0]) * f
            z = a[2] + (b[2] - a[2]) * f
            y = a[1] + vy * t - 0.5 * GRAVITY * t * t
            if f > 0.97:
                break
            if terrain.h(x, z) > y + 0.02:
                ok = False
                break
        if ok:
            return deg
    return None


# ---- level -> graph ------------------------------------------------------------------------

def rec_id(rec):
    return rec["id"] if isinstance(rec, dict) else rec[0]


def rec_pos(rec, key="pos"):
    return rec[key] if isinstance(rec, dict) else rec[1]


def rec_tag(rec) -> str:
    return (rec["tag"] if isinstance(rec, dict) else rec[2]).lower()


def key2(p) -> tuple:
    return (round(p[0], 3), round(p[2], 3))


def key3(p) -> tuple:
    return (round(p[0], 3), round(p[1], 3), round(p[2], 3))


def derive_edges(routes: list, names: dict) -> tuple[list, dict]:
    """The lane graph as DFLaneGraphBuilder::Derive builds it: a waypoint is a node where a route
    starts or ends, at either end of a warp, or where routes disagree about how they pass through
    it; two different spans between the same pair of nodes get their middle waypoint promoted
    (step 2b) so an itinerary can say which it means; identical spans are one edge."""
    context: dict = {}
    for r in routes:
        pts = [key3(p) for p in r["waypoints"]]
        legs = set(r.get("teleportLegs") or [])
        for i, k in enumerate(pts):
            ctx = context.setdefault(k, {"node": False, "ways": set()})
            if i == 0 or i == len(pts) - 1 or (i - 1) in legs or i in legs:
                ctx["node"] = True
            ctx["ways"].add((pts[i - 1] if i > 0 else None, pts[i + 1] if i + 1 < len(pts) else None))
    nodes = {k for k, c in context.items() if c["node"] or len(c["ways"]) > 1}
    for _ in range(8):
        spans: dict = {}
        for r in routes:
            pts = [key3(p) for p in r["waypoints"]]
            start = 0
            for i in range(1, len(pts)):
                if pts[i] in nodes:
                    spans.setdefault((pts[start], pts[i], r["layer"]), []).append(tuple(pts[start:i + 1]))
                    start = i
        promoted = 0
        for group in spans.values():
            if len(set(group)) < 2:
                continue
            for shape in group:
                if len(shape) - 1 >= 2 and shape[(len(shape) - 1) // 2] not in nodes:
                    nodes.add(shape[(len(shape) - 1) // 2])
                    promoted += 1
        if not promoted:
            break
    edges: list = []
    seen: dict = {}
    itineraries: dict = {}
    for r in routes:
        wps = r["waypoints"]
        pts = [key3(p) for p in wps]
        legs = set(r.get("teleportLegs") or [])
        via = []
        start = 0
        for i in range(1, len(pts)):
            if pts[i] not in nodes:
                continue
            span = tuple(pts[start:i + 1])
            if span not in seen:
                a = names.get(pts[start], f"n{pts[start]}")
                b = names.get(pts[i], f"n{pts[i]}")
                eid = f"{a}-{b}"
                if any(e["id"] == eid for e in edges):
                    eid = f"{eid}.{sum(1 for e in edges if e['id'].startswith(eid))}"
                seen[span] = len(edges)
                edges.append({"id": eid, "layer": r["layer"], "warp": (i - 1) in legs and i - start == 1,
                              "from": pts[start], "to": pts[i], "authored": [wps[j] for j in range(start, i + 1)],
                              "fromSpawn": start == 0})
            via.append(edges[seen[span]]["id"])
            start = i
        itineraries[r["id"]] = via
    return edges, itineraries


def resample(points3: list, spacing: float = 1.0) -> list:
    out = [points3[0]]
    for a, b in zip(points3, points3[1:]):
        n = max(1, int(math.ceil(math.dist((a[0], a[2]), (b[0], b[2])) / spacing)))
        for i in range(1, n + 1):
            t = i / n
            out.append(tuple(a[j] + (b[j] - a[j]) * t for j in range(3)))
    return out


def polyline_len(pts: list) -> float:
    return sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))


def point_along(pts: list, d: float) -> tuple:
    walked = 0.0
    for a, b in zip(pts, pts[1:]):
        leg = math.dist(a, b)
        if walked + leg >= d:
            t = (d - walked) / leg if leg > 0 else 0.0
            return tuple(a[j] + (b[j] - a[j]) * t for j in range(3))
        walked += leg
    return pts[-1]


def dist_to_polyline(p: tuple, pts: list) -> float:
    best = float("inf")
    for a, b in zip(pts, pts[1:]):
        ab = [b[j] - a[j] for j in range(3)]
        ap = [p[j] - a[j] for j in range(3)]
        L2 = sum(c * c for c in ab)
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, sum(ab[j] * ap[j] for j in range(3)) / L2))
        q = [a[j] + ab[j] * t for j in range(3)]
        best = min(best, math.dist(p, q))
    return best


# ---- the prediction --------------------------------------------------------------------------

def predict(map_id: str) -> tuple[dict, list, dict]:
    level_path = CONTENT / "levels" / f"{map_id}.level.json"
    terrain_path = CONTENT / "terrain" / f"{map_id}.terrain.json"
    level = json.loads(level_path.read_text())
    spec = json.loads(terrain_path.read_bytes())
    terrain = Terrain(spec)
    fails: list = []

    names = {key3(n["at"]): n["id"] for n in level.get("laneNodeNames", [])}
    routes = level["routes"]
    edges, itineraries = derive_edges(routes, names)

    # project: ground on the surface, air at surface + AGL (interpolated between authored waypoints)
    for e in edges:
        dense = resample(e["authored"], 1.0)
        if e["layer"] == "ground":
            e["path"] = [(p[0], terrain.h(p[0], p[2]), p[2]) for p in dense]
        else:
            e["path"] = [(p[0], terrain.h(p[0], p[2]) + p[1], p[2]) for p in dense]
            e["aglAuthored"] = [p[1] for p in dense]
        e["length"] = 0.0 if e["warp"] else polyline_len(e["path"])

    spawn_nodes = sorted({e["id"].split("-")[0]: e["path"][0] for e in edges if e["fromSpawn"]}.items())

    # sockets, projected like the importer (walls keep their authored height)
    sockets = []
    for s in level["sockets"]:
        x, y, z = rec_pos(s)
        tag = rec_tag(s)
        ground_y = terrain.h(x, z)
        py = y if tag == "wall" else ground_y
        sockets.append({"id": rec_id(s), "tag": tag, "pos": (x, py, z), "pad": bool(isinstance(s, dict) and s.get("pad")),
                        "groundSlope": r2(terrain.slope_percent(x, z)) if tag != "wall" else None})
    pads = [s for s in sockets if s["tag"] in ("ground", "wall")]
    walk_edges = [e for e in edges if not e["warp"]]
    ground_walk = [e for e in walk_edges if e["layer"] == "ground"]

    # --- rules the engine checks today
    offsets = []
    for s in pads:
        d, eid = min((dist_to_polyline(s["pos"], e["path"]), e["id"]) for e in walk_edges)
        s["offLane"] = r2(d)
        if d < SOCKET_OFF_LANE_M:
            offsets.append(f"socket {s['id']} is {d:.2f} m off {eid} (< 3.5 m)")
    fails += offsets
    apron = []
    for s in sockets:
        for n, p in spawn_nodes:
            d = math.dist(s["pos"], p)
            if d < SPAWN_APRON_M:
                apron.append(f"socket {s['id']} is {d:.1f} m from spawn {n} (< 8 m)")
    fails += apron

    segments = []
    for e in walk_edges:
        L = e["length"]
        count = max(1, math.ceil(L / SEGMENT_M))
        for k in range(count):
            d = min((k + 0.5) * SEGMENT_M, L)
            p = point_along(e["path"], d)
            seg = {"edge": e["id"], "t": round(d / L, 4), "layer": e["layer"], "apron": e["fromSpawn"] and d < SPAWN_APRON_M,
                   "positionMetres": [r2(c) for c in p], "coveringSockets": [], "blockedByTerrain": []}
            rng = AIR_RANGE_M if e["layer"] == "air" else GROUND_RANGE_M
            target = (p[0], p[1] + EYE_M, p[2])
            for s in pads:
                if math.dist(s["pos"], p) > rng:
                    continue
                muzzle = (s["pos"][0], s["pos"][1] + EYE_M, s["pos"][2])
                (seg["coveringSockets"] if terrain.clear(muzzle, target) else seg["blockedByTerrain"]).append(s["id"])
            segments.append(seg)
    dead = [s for s in segments if not s["apron"] and len(s["coveringSockets"]) < COVERAGE_REQUIRED]
    fails += [f"dead ground: {s['edge']} t={s['t']:.2f} ({s['layer']}) sees {len(s['coveringSockets'])} pad(s)" for s in dead]
    per_socket = {s["id"]: [f"{g['edge']}@{g['t']:.3f}" for g in segments if s["id"] in g["coveringSockets"]] for s in pads}
    idle = [sid for sid, seen in per_socket.items() if not seen]

    waves_path = CONTENT / "json" / f"waves_{map_id}.json"
    route_ids = []
    if waves_path.exists():
        wanted = {r.get("routeId") for r in json.loads(waves_path.read_text())["rows"]}
        missing = sorted(wanted - set(itineraries))
        fails += [f"{waves_path.name} names routeId {m!r}, which is not an itinerary" for m in missing]
        route_ids = sorted(wanted)

    # --- design rules the engine does not implement yet
    grades = []
    for e in ground_walk:
        pts = e["path"]
        worst, uphill, downhill = 0.0, 0.0, 0.0
        for a, b in zip(pts, pts[1:]):
            run = math.hypot(b[0] - a[0], b[2] - a[2])
            if run < 1e-6:
                continue
            g = (b[1] - a[1]) / run * 100.0
            worst = max(worst, abs(g))
            if g > SLOPE_BAND:
                uphill += run
            elif g < -SLOPE_BAND:
                downhill += run
        grades.append({"edge": e["id"], "lengthMetres": r2(e["length"]), "maxGradePercent": r2(worst),
                       "slowedMetres": r2(uphill), "hastenedMetres": r2(downhill),
                       "rise": r2(pts[-1][1] - pts[0][1])})
        if worst > LANE_MAX_GRADE:
            fails.append(f"lane grade: {e['id']} reaches {worst:.1f} % (> 30 %)")

    boss = level.get("bossRoute")
    boss_report = None
    if boss:
        via = itineraries.get(boss["routeId"], [])
        worst = max((g["maxGradePercent"] for g in grades if g["edge"] in via), default=0.0)
        warps = [eid for eid in via if next(e for e in edges if e["id"] == eid)["warp"]]
        boss_report = {"routeId": boss["routeId"], "via": via, "maxGradePercent": worst, "warpLegs": warps}
        if worst > BOSS_MAX_GRADE or warps:
            fails.append(f"boss route {boss['routeId']}: max grade {worst} % (<= 15 %), warps {warps}")
        nests = []
        boss_pts = [p for eid in via for p in next(e for e in edges if e["id"] == eid)["path"]]
        for n in level.get("nests", []):
            x, y, z = n["at"]
            eye = (x, max(y, terrain.h(x, z)) + EYE_M, z)
            seen = sum(1 for p in boss_pts if terrain.clear(eye, (p[0], p[1] + EYE_M, p[2])))
            nests.append({"id": n["id"], "overlookFraction": round(seen / max(1, len(boss_pts)), 3)})
        boss_report["nests"] = nests
        if nests and max(n["overlookFraction"] for n in nests) < 0.5:
            fails.append("boss route: no nest overlooks >= 50 % of it (rule 13)")

    pad_slopes = [{"id": s["id"], "groundSlopePercent": s["groundSlope"]} for s in sockets if s["groundSlope"] is not None and s["groundSlope"] > PAD_UNGRADED_MAX]
    fails += [f"socket {p['id']} stands on {p['groundSlopePercent']} % ground before the cut (> 25 %)" for p in pad_slopes]

    air = []
    for e in walk_edges:
        if e["layer"] != "air":
            continue
        worst_dev, min_clear = 0.0, float("inf")
        for p, agl in zip(e["path"], e["aglAuthored"]):
            ground = terrain.h(p[0], p[2])
            worst_dev = max(worst_dev, abs((p[1] - ground) - agl))
            min_clear = min(min_clear, p[1] - ground)
        # the same lane if an importer only lifts the authored waypoints and draws straight lines between them
        lifted = [(p[0], terrain.h(p[0], p[2]) + p[1], p[2]) for p in e["authored"]]
        straight_worst, straight_clear = 0.0, float("inf")
        for p, agl in zip(resample(lifted, 1.0), e["aglAuthored"]):
            ground = terrain.h(p[0], p[2])
            straight_worst = max(straight_worst, abs((p[1] - ground) - agl))
            straight_clear = min(straight_clear, p[1] - ground)
        air.append({"edge": e["id"], "lengthMetres": r2(e["length"]), "minAglMetres": r2(min_clear),
                    "ifStraightBetweenWaypoints": {"worstAglDeviationMetres": r2(straight_worst), "minClearanceMetres": r2(straight_clear)}})
        if min_clear < AIR_CLEARANCE_M:
            fails.append(f"air {e['id']}: clearance {min_clear:.1f} m (< 3 m)")

    ground_route_pts = [p for r in routes if r["layer"] == "ground" for eid in itineraries[r["id"]] for p in next(e for e in edges if e["id"] == eid)["path"]]
    relief = max(p[1] for p in ground_route_pts) - min(p[1] for p in ground_route_pts) if ground_route_pts else 0.0
    if relief < RELIEF_REQUIRED_M:
        fails.append(f"relief along the lane is {relief:.1f} m (G2: >= 8 m)")

    proof, g2 = [], []
    for g in segments:
        if g["layer"] != "ground" or g["apron"]:
            continue
        target = tuple(g["positionMetres"])
        for sid in g["blockedByTerrain"]:
            s = next(x for x in pads if x["id"] == sid)
            muzzle = (s["pos"][0], s["pos"][1] + EYE_M, s["pos"][2])
            flat = math.hypot(target[0] - muzzle[0], target[2] - muzzle[2])
            if not (TOWERS["nova"]["min"] <= flat <= TOWERS["nova"]["range"]):
                continue
            nova = arc_clear(terrain, muzzle, target, TOWERS["nova"]["speed"], TOWERS["nova"]["pitch"])
            if nova is None:
                continue
            lance_pitch = math.degrees(math.atan2(target[1] + EYE_M - muzzle[1], flat))
            pair = {"socket": sid, "edge": g["edge"], "t": g["t"], "distanceMetres": r2(flat),
                    "lancePitchDeg": r2(lance_pitch), "novaLaunchDeg": r2(nova)}
            proof.append(pair)
            if flat <= TOWERS["lance"]["range"] and TOWERS["lance"]["pitch"][0] <= lance_pitch <= TOWERS["lance"]["pitch"][1]:
                g2.append(pair)
    if not proof:
        fails.append("terrain proof (rule 16): no pad has LOS to a lane segment blocked by terrain and recovered by Nova's arc")
    if not g2:
        fails.append("G2 check 2: no pad where a Lance is in range and within its pitch limits but blocked by terrain while a Nova's arc clears")

    # rule 14 (not in DF.Map.Validate yet): no pad sees into a ground spawn portal. Terrain only: the
    # portal mesh itself may add shadow, so this is reported, not failed.
    portal_seen = []
    ground_spawns = {e["id"].split("-")[0] for e in edges if e["fromSpawn"] and e["layer"] == "ground"}
    for n, p in spawn_nodes:
        if n not in ground_spawns:
            continue
        eye = (p[0], p[1] + 1.0, p[2])
        for s in pads:
            if terrain.clear((s["pos"][0], s["pos"][1] + EYE_M, s["pos"][2]), eye):
                portal_seen.append(f"{s['id']} sees spawn {n} ({math.dist(s['pos'], p):.1f} m)")

    lengths = {r["id"]: r2(sum(next(e for e in edges if e["id"] == eid)["length"] for eid in itineraries[r["id"]])) for r in routes}
    counts: dict = {}
    for s in sockets:
        counts[s["tag"]] = counts.get(s["tag"], 0) + 1
    traps_off = [f"{s['id']} {dist_to_polyline(s['pos'], [p for e in ground_walk for p in e['path']]):.1f} m" for s in sockets
                 if s["tag"] == "trap" and dist_to_polyline(s["pos"], [p for e in ground_walk for p in e["path"]]) > 1.7]
    half = (level["field"][0] / 2.0, level["field"][1] / 2.0)
    outside = [s["id"] for s in sockets if abs(s["pos"][0]) > half[0] - 1.5 or abs(s["pos"][2]) > half[1] - 1.5]

    report = {
        "map": map_id,
        "note": "PREDICTED by tools/ue-bridge/terrain/predict_level.py from the terrain spec alone (no decks, no dressing, no pad cut, no navmesh). The engine's reports/<map>.coverage.json is the measurement; this is the design target it should reproduce.",
        "inputs": {"terrainSha256": hashlib.sha256(terrain_path.read_bytes()).hexdigest(),
                   "levelSha256": hashlib.sha256(level_path.read_bytes()).hexdigest(),
                   "assumed": {"novaSpeed": TOWERS["nova"]["speed"], "novaGravity": GRAVITY}},
        "summary": {
            "segments": len(segments), "apron": sum(1 for s in segments if s["apron"]), "dead": len(dead),
            "socketCounts": counts, "routeLengthsMetres": lengths, "itineraries": itineraries,
            "reliefOnLaneMetres": r2(relief), "idlePads": idle, "trapsOffLane": traps_off, "socketsNearEdge": outside,
            "routeIdsInWaves": route_ids,
        },
        "rules": {"socketOffset": offsets, "spawnApron": apron, "padGroundSlope": pad_slopes, "spawnShadowTerrainOnly": portal_seen},
        "grades": grades,
        "boss": boss_report,
        "air": air,
        "terrainProof": {"rule16Count": len(proof), "rule16": proof[:40],
                         "g2LanceBlockedNovaClears": g2,
                         "note": "rule16: a pad whose sight line (pad + 1.6 m to segment + 1.6 m) is blocked by terrain and whose Nova arc (5-16 m, launch 22-70 deg) clears it. g2: the subset where a Lance on that pad would be in range (12 m) and inside its pitch limits (-8..26 deg), so LOS is the only reason it cannot fire."},
        "sockets": [{"id": s["id"], "tag": s["tag"], "positionMetres": [r2(c) for c in s["pos"]], "groundSlopePercent": s["groundSlope"],
                     "offLaneMetres": s.get("offLane"), "covers": len(per_socket.get(s["id"], []))} for s in sockets],
        "segments": segments,
        "deadSegments": [{"edge": s["edge"], "t": s["t"], "count": len(s["coveringSockets"])} for s in dead],
    }
    return report, fails, {"terrain": terrain, "edges": edges, "sockets": sockets, "segments": segments}


# ---- overlay ---------------------------------------------------------------------------------

def write_overlay(path: Path, level: dict, ctx: dict, scale: int = 4) -> None:
    terrain = ctx["terrain"]
    g = terrain.grid
    half = (level["field"][0] / 2.0 + 6, level["field"][1] / 2.0 + 6)
    W, H = int(2 * half[0] * scale), int(2 * half[1] * scale)
    lo, hi = min(g.h), max(g.h)
    img = bytearray(W * H * 3)

    def to_px(x, z):
        return int((x + half[0]) * scale), int((half[1] - z) * scale)   # north (+z) up

    for py in range(H):
        z = half[1] - (py + 0.5) / scale
        for px in range(W):
            x = -half[0] + (px + 0.5) / scale
            y = terrain.h(x, z)
            shade = (terrain.h(x - 0.5, z + 0.5) - terrain.h(x + 0.5, z - 0.5)) * 60
            v = 70 + (y - lo) / (hi - lo) * 150 + shade
            if abs((y - 1.0) / 2.0 - round((y - 1.0) / 2.0)) < 0.04:   # odd metres: the terraces sit on even ones
                v -= 35
            v = max(0, min(255, int(v)))
            i = (py * W + px) * 3
            img[i:i + 3] = bytes((v, v, v))

    def dot(x, z, rgb, r):
        cx, cy = to_px(x, z)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r and 0 <= cx + dx < W and 0 <= cy + dy < H:
                    i = ((cy + dy) * W + cx + dx) * 3
                    img[i:i + 3] = bytes(rgb)

    for e in ctx["edges"]:
        colour = (60, 170, 255) if e["layer"] == "air" else (230, 60, 60)
        for p in e["path"]:
            dot(p[0], p[2], colour, 2 if e["layer"] == "air" else 3)
    for s in ctx["segments"]:
        n = len(s["coveringSockets"])
        colour = (120, 120, 120) if s["apron"] else ((40, 200, 90) if n >= COVERAGE_REQUIRED else (255, 230, 0))
        dot(s["positionMetres"][0], s["positionMetres"][2], colour, 2)
    tag_colour = {"ground": (255, 140, 0), "wall": (200, 80, 255), "trap": (255, 255, 255), "barricade": (240, 200, 60)}
    for s in ctx["sockets"]:
        dot(s["pos"][0], s["pos"][2], tag_colour[s["tag"]], 4 if s["tag"] != "trap" else 2)
    raw = b"".join(b"\x00" + bytes(img[y * W * 3:(y + 1) * W * 3]) for y in range(H))
    png = b"\x89PNG\r\n\x1a\n" + bh._chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) + \
        bh._chunk(b"IDAT", zlib.compress(raw, 9)) + bh._chunk(b"IEND", b"")
    path.write_bytes(png)


def suggest(ctx: dict, level: dict, count: int) -> list:
    """The best spots for new ground pads against the current dead ground: flat (<= 18 % before the
    cut), >= 4 m off every lane, >= 8.5 m from a spawn, >= 3.5 m from another pad, 1.5 m inside
    the field; ranked by how many dead segments each would add a covering pad to."""
    terrain, segments = ctx["terrain"], ctx["segments"]
    walk = [e for e in ctx["edges"] if not e["warp"]]
    spawns = [e["path"][0] for e in walk if e["fromSpawn"]]
    dead = [s for s in segments if not s["apron"] and len(s["coveringSockets"]) < COVERAGE_REQUIRED]
    taken = [s["pos"] for s in ctx["sockets"]]
    half = (level["field"][0] / 2.0 - 1.5, level["field"][1] / 2.0 - 1.5)
    out = []
    for xi in range(-int(half[0]), int(half[0]) + 1):
        for zi in range(-int(half[1]), int(half[1]) + 1):
            x, z = float(xi), float(zi)
            if terrain.slope_percent(x, z) > 18.0:
                continue
            p = (x, terrain.h(x, z), z)
            if any(math.dist(p, q) < 8.5 for q in spawns) or any(math.hypot(x - q[0], z - q[2]) < 3.5 for q in taken):
                continue
            if min(dist_to_polyline(p, e["path"]) for e in walk) < 4.0:
                continue
            muzzle = (x, p[1] + EYE_M, z)
            fixes = [f"{s['edge']}@{s['t']:.2f}" for s in dead
                     if math.dist(p, s["positionMetres"]) <= (AIR_RANGE_M if s["layer"] == "air" else GROUND_RANGE_M)
                     and terrain.clear(muzzle, (s["positionMetres"][0], s["positionMetres"][1] + EYE_M, s["positionMetres"][2]))]
            if fixes:
                out.append((len(fixes), xi, zi, fixes))
    out.sort(key=lambda c: (-c[0], c[1], c[2]))
    return out[:count]


def main(argv: list | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("map")
    ap.add_argument("--out", type=Path, default=None, help="report path (default unreal/content/levels/reports/<map>.predicted.json)")
    ap.add_argument("--png", type=Path, default=None, help="also write an overlay: lane red, air blue, segments green/yellow(dead)/grey(apron), pads orange, wall pads violet, traps white")
    ap.add_argument("--suggest", type=int, default=0, metavar="N", help="also print the N best spots for a new ground pad against the dead ground")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args(argv)
    report, fails, ctx = predict(a.map)
    out = a.out or (CONTENT / "levels" / "reports" / f"{a.map}.predicted.json")
    out.write_text(json.dumps(report, indent=1, sort_keys=True) + "\n")
    if a.png:
        write_overlay(a.png, json.loads((CONTENT / "levels" / f"{a.map}.level.json").read_text()), ctx)
    s = report["summary"]
    print(f"{a.map}: {s['segments']} segments ({s['apron']} apron), {s['dead']} dead; sockets {s['socketCounts']}; "
          f"relief on lane {s['reliefOnLaneMetres']} m; routes {s['routeLengthsMetres']}")
    if not a.quiet:
        for g in report["grades"]:
            print(f"  grade {g['edge']}: {g['lengthMetres']} m, max {g['maxGradePercent']} %, slowed {g['slowedMetres']} m, hastened {g['hastenedMetres']} m, rise {g['rise']} m")
        if report["boss"]:
            print(f"  boss {report['boss']}")
        for x in report["air"]:
            print(f"  air {x}")
        tp = report["terrainProof"]
        print(f"  terrain proof: rule 16 pairs {tp['rule16Count']}; G2 (Lance blocked only by terrain, Nova clears) {len(tp['g2LanceBlockedNovaClears'])}: {tp['g2LanceBlockedNovaClears'][:4]}")
        print(f"  rule 14 (terrain only): {report['rules']['spawnShadowTerrainOnly'] or 'no pad sees a spawn'}")
        print(f"  idle pads: {s['idlePads']}; traps off lane: {s['trapsOffLane']}; near edge: {s['socketsNearEdge']}")
    for f in fails:
        print("  FAIL", f)
    if a.suggest:
        level = json.loads((CONTENT / "levels" / f"{a.map}.level.json").read_text())
        for n, x, z, fixes in suggest(ctx, level, a.suggest):
            print(f"  suggest ({x}, {z}): +1 pad on {n} dead segment(s): {', '.join(fixes[:6])}")
    print(f"wrote {out}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
