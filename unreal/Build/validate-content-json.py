#!/usr/bin/env python3
"""Validate unreal/content against unreal/content/schema/*.schema.json.

Three families of file:
  json/<table>.json                 against <table>.schema.json (waves_<map> against waves.schema.json),
                                    plus envelope `table` and duplicate row ids
  levels/<map>.level.json,          against level.schema.json, plus the rules a schema cannot say:
  levels/legacy/<map>.level.json    unique ids, teleport legs, gate sockets, condition ids, the boss
                                    route, and every routeId in waves_<map>.json resolving to a route
                                    (map-authoring-3d.md, "Route ids are referenced by the wave tables")
  terrain/<map>.terrain.json        against terrain.schema.json, plus unique feature ids and noise masks
                                    that name a real, earlier feature

A deliberately small JSON-Schema subset, no external dependency, so it runs on any machine and in CI:
type, enum, const, required, properties, additionalProperties (bool or schema), items, prefixItems,
propertyNames, minItems, maxItems, minimum, maximum, minLength, pattern, oneOf, anyOf, and local
$ref ("#/$defs/<name>").

  python3 unreal/Build/validate-content-json.py            # everything
  python3 unreal/Build/validate-content-json.py towers     # one table
  python3 unreal/Build/validate-content-json.py foundry    # every file for one map (level, legacy, terrain, waves)
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CONTENT = REPO / "unreal" / "content"
JSON_DIR = CONTENT / "json"
SCHEMA_DIR = CONTENT / "schema"
LEVELS_DIR = CONTENT / "levels"
TERRAIN_DIR = CONTENT / "terrain"

TYPES = {
    "string": lambda v: isinstance(v, str),
    "boolean": lambda v: isinstance(v, bool),
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "array": lambda v: isinstance(v, list),
    "object": lambda v: isinstance(v, dict),
    "null": lambda v: v is None,
}


def resolve(schema: dict, root: dict) -> dict:
    while "$ref" in schema:
        ref = schema["$ref"]
        if not ref.startswith("#/"):
            raise ValueError(f"only local $ref is supported, got {ref!r}")
        target = root
        for part in ref[2:].split("/"):
            target = target[part]
        schema = target
    return schema


def mismatch_rank(value, schema: dict, root: dict) -> int:
    """How clearly a oneOf/anyOf branch is the wrong branch: 2 = wrong type, 1 = a `const`
    property (the discriminator: `$schema`, `layer`) differs, 0 = plausibly the intended branch."""
    schema = resolve(schema, root)
    t = schema.get("type")
    if t and not any(TYPES[x](value) for x in (t if isinstance(t, list) else [t])):
        return 2
    if isinstance(value, dict):
        for k, sub in schema.get("properties", {}).items():
            sub = resolve(sub, root)
            if "const" in sub and k in value and value[k] != sub["const"]:
                return 1
    return 0


def check(value, schema: dict, path: str, errors: list[str], root: dict | None = None) -> None:
    root = root if root is not None else schema
    schema = resolve(schema, root)
    for key in ("oneOf", "anyOf"):
        if key not in schema:
            continue
        results = []
        for branch in schema[key]:
            errs: list[str] = []
            check(value, branch, path, errs, root)
            results.append((mismatch_rank(value, branch, root), len(errs), errs))
        passing = [r for r in results if not r[2]]
        if key == "oneOf" and len(passing) > 1:
            errors.append(f"{path}: matches {len(passing)} branches of oneOf, expected exactly one")
        if not passing:
            errors.extend(min(results, key=lambda r: (r[0], r[1]))[2])
    if "const" in schema and value != schema["const"]:
        errors.append(f"{path}: expected const {schema['const']!r}, got {value!r}")
    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{path}: {value!r} not in {schema['enum']}")
    t = schema.get("type")
    if t:
        types = t if isinstance(t, list) else [t]
        if not any(TYPES[x](value) for x in types):
            errors.append(f"{path}: expected {t}, got {type(value).__name__}")
            return
    if TYPES["number"](value):
        if "minimum" in schema and value < schema["minimum"]:
            errors.append(f"{path}: {value} is below the minimum {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            errors.append(f"{path}: {value} is above the maximum {schema['maximum']}")
    if isinstance(value, str):
        if "minLength" in schema and len(value) < schema["minLength"]:
            errors.append(f"{path}: shorter than {schema['minLength']} characters")
        if "pattern" in schema and not re.search(schema["pattern"], value):
            errors.append(f"{path}: {value!r} does not match {schema['pattern']}")
    if isinstance(value, dict):
        for r in schema.get("required", []):
            if r not in value:
                errors.append(f"{path}: missing required '{r}'")
        props = schema.get("properties", {})
        extra = schema.get("additionalProperties", True)
        names = schema.get("propertyNames")
        for k, v in value.items():
            if names is not None:
                check(k, names, f"{path}.{k}<key>", errors, root)
            if k in props:
                check(v, props[k], f"{path}.{k}", errors, root)
            elif extra is False:
                errors.append(f"{path}: unknown key '{k}'")
            elif isinstance(extra, dict):
                check(v, extra, f"{path}.{k}", errors, root)
    elif isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            errors.append(f"{path}: fewer than {schema['minItems']} items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            errors.append(f"{path}: more than {schema['maxItems']} items")
        prefix = schema.get("prefixItems", [])
        for i, sub in enumerate(prefix[: len(value)]):
            check(value[i], sub, f"{path}[{i}]", errors, root)
        items = schema.get("items")
        if isinstance(items, dict):
            for i, v in enumerate(value[len(prefix):], start=len(prefix)):
                check(v, items, f"{path}[{i}]", errors, root)


def load_schema(name: str) -> dict | None:
    p = SCHEMA_DIR / f"{name}.schema.json"
    return json.loads(p.read_text()) if p.exists() else None


def report(label: str, errors: list[str], summary: str) -> int:
    if errors:
        print(f"FAIL {label}")
        for e in errors[:20]:
            print("   ", e)
        if len(errors) > 20:
            print(f"    ... and {len(errors) - 20} more")
        return 1
    print(f"ok   {label} ({summary})")
    return 0


# ---- content tables -------------------------------------------------------------------------

def validate_table(f: Path) -> int:
    table = f.stem
    schema_name = "waves" if table.startswith("waves_") else table
    schema = load_schema(schema_name)
    if schema is None:
        print(f"{table}: no schema ({schema_name}.schema.json)")
        return 1
    doc = json.loads(f.read_text())
    errors: list[str] = []
    check(doc, schema, table, errors)
    if doc.get("table") != table:
        errors.append(f"{table}: envelope 'table' is {doc.get('table')!r}")
    ids = [r.get("id") for r in doc.get("rows", [])]
    dupes = {i for i in ids if ids.count(i) > 1}
    if dupes:
        errors.append(f"{table}: duplicate ids {sorted(dupes)}")
    return report(f"{table}.json", errors, f"{len(ids)} rows")


# ---- level files ----------------------------------------------------------------------------

def record_id(rec) -> str | None:
    if isinstance(rec, dict):
        return rec.get("id")
    if isinstance(rec, list) and rec and isinstance(rec[0], str):
        return rec[0]
    return None


def socket_tag(rec) -> str:
    tag = rec.get("tag") if isinstance(rec, dict) else (rec[2] if isinstance(rec, list) and len(rec) > 2 else "")
    return str(tag).lower()


def dupes(ids: list) -> list:
    return sorted({i for i in ids if i is not None and ids.count(i) > 1})


def level_rules(doc: dict, name: str, map_id: str, errors: list[str]) -> None:
    """What a schema cannot say about one level file."""
    if doc.get("id") != map_id:
        errors.append(f"{name}: id is {doc.get('id')!r}, the file name says {map_id!r}")
    routes = [r for r in doc.get("routes", []) if isinstance(r, dict)]
    sockets = doc.get("sockets", [])
    stations = (doc.get("anchors") or {}).get("stations", doc.get("stations", []))
    route_ids = [r.get("id") for r in routes]
    for kind, ids in (("route", route_ids), ("socket", [record_id(s) for s in sockets]),
                      ("station", [record_id(s) for s in stations])):
        if dupes(ids):
            errors.append(f"{name}: duplicate {kind} ids {dupes(ids)}")

    for r in routes:
        legs = r.get("teleportLegs") or []
        n = len(r.get("waypoints", [])) - 1
        layer = str(r.get("layer", "")).lower()
        if legs and layer == "air":
            errors.append(f"{name}: route {r.get('id')!r}: an air route may not teleport")
        for leg in legs:
            if not isinstance(leg, int) or leg <= 0 or leg >= n - 1:
                errors.append(f"{name}: route {r.get('id')!r}: teleport leg {leg} is the first or last leg, or out of range (legs 0..{n - 1})")
        if any(b - a == 1 for a, b in zip(sorted(legs), sorted(legs)[1:])):
            errors.append(f"{name}: route {r.get('id')!r}: two teleport legs in a row {sorted(legs)}")

    barricades = {record_id(s) for s in sockets if socket_tag(s) == "barricade"}
    for g in doc.get("laneGates", []) or []:
        if isinstance(g, dict) and g.get("socketId") not in barricades:
            errors.append(f"{name}: laneGate on {g.get('edgeId')!r} names {g.get('socketId')!r}, which is not a barricade socket")
    for node in doc.get("laneNodeNames", []) or []:
        if isinstance(node, dict) and "@" in str(node.get("id", "")):
            errors.append(f"{name}: lane node {node.get('id')!r} contains '@' (warp gate keys are \"<node>@<edge>\")")

    conditions = doc.get("conditions", doc.get("conditionSchedule")) or {}
    known = set()
    cond_path = JSON_DIR / "conditions.json"
    if cond_path.exists():
        known = {r.get("id") for r in json.loads(cond_path.read_text()).get("rows", [])}
    total = doc.get("totalWaves")
    for k, v in conditions.items():
        if known and v not in known:
            errors.append(f"{name}: condition {v!r} (wave index {k}) is not a conditions.json id {sorted(known)}")
        if isinstance(total, int) and k.isdigit() and int(k) >= total:
            errors.append(f"{name}: condition at wave index {k} but totalWaves is {total} (indices are 0-based)")

    boss = doc.get("bossRoute")
    if isinstance(boss, dict):
        target = next((r for r in routes if r.get("id") == boss.get("routeId")), None)
        if target is None:
            errors.append(f"{name}: bossRoute names {boss.get('routeId')!r}, which is not a route")
        else:
            if str(target.get("layer", "")).lower() != "ground":
                errors.append(f"{name}: bossRoute {boss.get('routeId')!r} is not a ground route")
            if target.get("teleportLegs"):
                errors.append(f"{name}: bossRoute {boss.get('routeId')!r} has warp legs (rule 13)")
    for nest in doc.get("nests", []) or []:
        for rid in nest.get("overlooks", []) if isinstance(nest, dict) else []:
            if rid not in route_ids:
                errors.append(f"{name}: nest {nest.get('id')!r} overlooks {rid!r}, which is not a route")

    field = doc.get("field")
    terrain_path = TERRAIN_DIR / f"{map_id}.terrain.json"
    if doc.get("$schema") == "deepfield-level/1" and terrain_path.exists() and isinstance(field, list):
        try:
            playable = json.loads(terrain_path.read_text()).get("bounds", {}).get("playable")
        except json.JSONDecodeError:
            playable = None
        if playable is not None and [float(x) for x in playable] != [float(x) for x in field]:
            errors.append(f"{name}: field {field} differs from {terrain_path.name} bounds.playable {playable}")


def wave_routes(map_id: str) -> tuple[Path, set] | None:
    p = JSON_DIR / f"waves_{map_id}.json"
    if not p.exists():
        return None
    rows = json.loads(p.read_text()).get("rows", [])
    return p, {r.get("routeId") for r in rows if r.get("routeId")}


def validate_levels(only: set) -> int:
    schema = load_schema("level")
    if schema is None:
        print("levels: no schema (level.schema.json)")
        return 1
    failed = 0
    primaries = {p.name.removesuffix(".level.json"): p for p in sorted(LEVELS_DIR.glob("*.level.json"))}
    legacies = {p.name.removesuffix(".level.json"): p for p in sorted((LEVELS_DIR / "legacy").glob("*.level.json"))}
    for map_id in sorted(set(primaries) | set(legacies)):
        if only and map_id not in only and "levels" not in only:
            continue
        for path in (legacies.get(map_id), primaries.get(map_id)):
            if path is None:
                continue
            name = path.relative_to(CONTENT).as_posix()
            try:
                doc = json.loads(path.read_text())
            except json.JSONDecodeError as e:
                failed += report(name, [f"{name}: not valid JSON: {e}"], "")
                continue
            errors: list[str] = []
            check(doc, schema, name, errors)
            level_rules(doc, name, map_id, errors)
            # The wave table binds to the file the importer would load: the primary once it exists.
            if path == (primaries.get(map_id) or legacies.get(map_id)):
                waves = wave_routes(map_id)
                if waves:
                    wpath, wanted = waves
                    have = {r.get("id") for r in doc.get("routes", []) if isinstance(r, dict)}
                    for rid in sorted(wanted - have):
                        errors.append(f"{name}: {wpath.name} names routeId {rid!r}, which is not a route in {path.name} (routes: {sorted(have)})")
            n_sock = len(doc.get("sockets", []))
            failed += report(name, errors, f"{len(doc.get('routes', []))} routes, {n_sock} sockets")
    return failed


# ---- terrain files --------------------------------------------------------------------------

def validate_terrain(only: set) -> int:
    schema = load_schema("terrain")
    if schema is None:
        print("terrain: no schema (terrain.schema.json)")
        return 1
    failed = 0
    for path in sorted(TERRAIN_DIR.glob("*.terrain.json")):
        map_id = path.name.removesuffix(".terrain.json")
        if only and map_id not in only and "terrain" not in only:
            continue
        name = path.relative_to(CONTENT).as_posix()
        doc = json.loads(path.read_text())
        errors: list[str] = []
        check(doc, schema, name, errors)
        if doc.get("id") != map_id:
            errors.append(f"{name}: id is {doc.get('id')!r}, the file name says {map_id!r}")
        features = doc.get("features", [])
        ids = [f.get("id") for f in features] + [n.get("id") for n in doc.get("noise", [])] + \
              [f.get("id") for f in doc.get("flats", [])]
        if dupes(ids):
            errors.append(f"{name}: duplicate ids {dupes(ids)}")
        prio = {f.get("id"): f.get("priority", 0) for f in features if f.get("id")}
        for layer in doc.get("noise", []):
            mask = layer.get("mask")
            ref = mask if isinstance(mask, str) and mask != "all" else (mask.get("feature") if isinstance(mask, dict) else None)
            if ref is None:
                continue
            if ref not in prio:
                errors.append(f"{name}: noise {layer.get('id')!r} masks by feature {ref!r}, which does not exist")
            elif prio[ref] >= layer.get("priority", 50):
                errors.append(f"{name}: noise {layer.get('id')!r} masks by {ref!r}, whose priority {prio[ref]} is not below the layer's {layer.get('priority', 50)}")
        failed += report(name, errors, f"{len(features)} features")
        # The generated heightmap is what DFTerrainImport reads. It is LFS and WS-30's lane, so a
        # spec change can land before it is regenerated; say so loudly, but it is not a schema error.
        built = TERRAIN_DIR / "out" / f"{map_id}_height.json"
        if built.exists():
            want = hashlib.sha256(path.read_bytes()).hexdigest()
            have = json.loads(built.read_text()).get("sourceSha256")
            if have != want:
                print(f"warn {built.relative_to(CONTENT).as_posix()} was built from another {path.name} "
                      f"(sourceSha256 {str(have)[:12]}…, spec is {want[:12]}…): the outputs are stale; "
                      f"run python3 tools/ue-bridge/terrain/build_heightmap.py {map_id} before -run=DFTerrainImport")
    return failed


def main() -> int:
    only = set(sys.argv[1:])
    files = sorted(JSON_DIR.glob("*.json"))
    if not files:
        print(f"no content json under {JSON_DIR}")
        return 1
    failed = 0
    for f in files:
        table = f.stem
        map_of_waves = table.removeprefix("waves_") if table.startswith("waves_") else None
        if only and table not in only and map_of_waves not in only:
            continue
        failed += validate_table(f)
    failed += validate_levels(only)
    failed += validate_terrain(only)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
