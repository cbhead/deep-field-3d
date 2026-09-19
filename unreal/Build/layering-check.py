#!/usr/bin/env python3
"""Fail if any DF module's Build.cs depends on a DF module at the same or a higher layer.

Layers come from unreal/Build/modules.json. Run before every PR (Section 6.5) and by INT.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "unreal" / "DeepField" / "Source"
MODULES = json.loads((REPO / "unreal" / "Build" / "modules.json").read_text())["modules"]


def main() -> int:
    bad = 0
    for name, spec in MODULES.items():
        build = SRC / name / f"{name}.Build.cs"
        if not build.exists():
            continue
        text = build.read_text()
        deps = set(re.findall(r'"(DF[A-Za-z]+)"', text)) - {name}
        for d in deps:
            if d not in MODULES:
                print(f"{name}: depends on unknown DF module {d}")
                bad += 1
            elif MODULES[d]["layer"] >= spec["layer"]:
                print(f"{name} (layer {spec['layer']}) -> {d} (layer {MODULES[d]['layer']}): upward or lateral dependency")
                bad += 1
    print("layering: OK" if bad == 0 else f"layering: {bad} violation(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
