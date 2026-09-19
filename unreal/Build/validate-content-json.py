#!/usr/bin/env python3
"""Validate unreal/content/json/*.json against unreal/content/schema/*.schema.json.

A deliberately small JSON-Schema subset (the one content-export emits): type, enum, const,
required, properties, additionalProperties (bool or schema), items, propertyNames, minItems,
maxItems. No external dependency so it runs on any machine and in CI.

  python3 unreal/Build/validate-content-json.py            # all tables
  python3 unreal/Build/validate-content-json.py towers     # one
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
JSON_DIR = REPO / "unreal" / "content" / "json"
SCHEMA_DIR = REPO / "unreal" / "content" / "schema"

TYPES = {
    "string": lambda v: isinstance(v, str),
    "boolean": lambda v: isinstance(v, bool),
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "array": lambda v: isinstance(v, list),
    "object": lambda v: isinstance(v, dict),
    "null": lambda v: v is None,
}


def check(value, schema: dict, path: str, errors: list[str]) -> None:
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
    if isinstance(value, dict):
        for r in schema.get("required", []):
            if r not in value:
                errors.append(f"{path}: missing required '{r}'")
        props = schema.get("properties", {})
        extra = schema.get("additionalProperties", True)
        names = schema.get("propertyNames")
        for k, v in value.items():
            if names is not None:
                check(k, names, f"{path}.{k}<key>", errors)
            if k in props:
                check(v, props[k], f"{path}.{k}", errors)
            elif extra is False:
                errors.append(f"{path}: unknown key '{k}'")
            elif isinstance(extra, dict):
                check(v, extra, f"{path}.{k}", errors)
    elif isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            errors.append(f"{path}: fewer than {schema['minItems']} items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            errors.append(f"{path}: more than {schema['maxItems']} items")
        items = schema.get("items")
        if isinstance(items, dict):
            for i, v in enumerate(value):
                check(v, items, f"{path}[{i}]", errors)


def main() -> int:
    only = set(sys.argv[1:])
    failed = 0
    files = sorted(JSON_DIR.glob("*.json"))
    if not files:
        print(f"no content json under {JSON_DIR}")
        return 1
    for f in files:
        table = f.stem
        if only and table not in only:
            continue
        schema_name = "waves" if table.startswith("waves_") else table
        schema_path = SCHEMA_DIR / f"{schema_name}.schema.json"
        if not schema_path.exists():
            print(f"{table}: no schema ({schema_path.name})")
            failed += 1
            continue
        doc = json.loads(f.read_text())
        schema = json.loads(schema_path.read_text())
        errors: list[str] = []
        check(doc, schema, table, errors)
        if doc.get("table") != table:
            errors.append(f"{table}: envelope 'table' is {doc.get('table')!r}")
        ids = [r.get("id") for r in doc.get("rows", [])]
        dupes = {i for i in ids if ids.count(i) > 1}
        if dupes:
            errors.append(f"{table}: duplicate ids {sorted(dupes)}")
        if errors:
            failed += 1
            print(f"FAIL {table}.json")
            for e in errors[:20]:
                print("   ", e)
        else:
            print(f"ok   {table}.json ({len(ids)} rows)")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
