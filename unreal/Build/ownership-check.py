#!/usr/bin/env python3
"""Check that a change set only touches paths its workstream owns (PROGRAMME.md §5.4, §6.9).

Reads the glob table in unreal/PLAN/OWNERSHIP.md. Binary files (the LFS set) outside the
workstream's globs are violations; text files outside are warnings (they should go through
the owner or an RFC, but a cross-cutting text PR is sometimes right — INT decides).

  python3 unreal/Build/ownership-check.py --ws 04                 # diff HEAD against origin/main
  python3 unreal/Build/ownership-check.py --ws INT --base origin/main  # INT may touch anything; still lists
  python3 unreal/Build/ownership-check.py --ws 10a --files a b c  # explicit paths
"""
from __future__ import annotations

import argparse
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OWNERSHIP = REPO / "unreal" / "PLAN" / "OWNERSHIP.md"
BINARY_EXT = {".uasset", ".umap", ".ubulk", ".uexp", ".upayload", ".uptnl", ".fbx", ".glb", ".gltf",
              ".png", ".tga", ".exr", ".hdr", ".psd", ".wav", ".ogg", ".mp3", ".ttf", ".otf"}
ROW = re.compile(r"^\|\s*(.+?)\s*\|\s*(.+?)\s*\|$")


def load_rules() -> list[tuple[str, str]]:
    rules: list[tuple[str, str]] = []
    for line in OWNERSHIP.read_text().splitlines():
        m = ROW.match(line)
        if not m or m.group(1).startswith("Glob") or set(m.group(1)) <= {"-"}:
            continue
        globs = re.findall(r"`([^`]+)`", m.group(1))
        owner = m.group(2)
        ws = re.findall(r"WS-\w+", owner)
        owner_id = "INT" if owner.startswith("INT") else (ws[0] if ws else owner.strip())
        for g in globs:
            for expanded in expand_braces(g.strip()):
                if not (expanded.startswith("unreal/") or expanded.startswith("tools/") or expanded.startswith(".") or expanded.startswith("docs/")):
                    expanded = "unreal/DeepField/" + expanded
                rules.append((expanded, owner_id))
    return rules


def expand_braces(glob: str) -> list[str]:
    """`a/{x,y}/b` -> [`a/x/b`, `a/y/b`] (nested braces expand recursively)."""
    m = re.search(r"\{([^{}]*)\}", glob)
    if not m:
        return [glob]
    out: list[str] = []
    for alt in m.group(1).split(","):
        out += expand_braces(glob[: m.start()] + alt.strip() + glob[m.end():])
    return out


def matches(path: str, glob: str) -> bool:
    # fnmatch's * already crosses '/'; map ** to * and let <Map>/<ws> placeholders match anything.
    g = glob.replace("**", "*").replace("<Map>", "*").replace("<ws>", "*").replace("<id>", "*")
    return fnmatch.fnmatch(path, g) or fnmatch.fnmatch(path, g.rstrip("/*") + "/*")


def owners_of(path: str, rules: list[tuple[str, str]]) -> list[str]:
    return [owner for glob, owner in rules if matches(path, glob)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ws", required=True, help="e.g. 04, 10a, INT")
    ap.add_argument("--base", default="origin/main")
    ap.add_argument("--files", nargs="*")
    a = ap.parse_args()
    me = "INT" if a.ws.upper() == "INT" else f"WS-{a.ws}"

    files = a.files
    if files is None:
        out = subprocess.run(["git", "diff", "--name-only", f"{a.base}...HEAD"], cwd=REPO, capture_output=True, text=True, check=True)
        files = [f for f in out.stdout.split("\n") if f]
    rules = load_rules()
    violations = warnings = 0
    for f in files:
        owners = owners_of(f, rules)
        mine = me == "INT" or me in owners or any(o.startswith(me) for o in owners)
        if mine:
            continue
        who = ", ".join(sorted(set(owners))) or "nobody (add a row to OWNERSHIP.md)"
        if Path(f).suffix.lower() in BINARY_EXT:
            print(f"VIOLATION {f}  (binary owned by {who})")
            violations += 1
        else:
            print(f"warning   {f}  (owned by {who}; PR to the owner or RFC)")
            warnings += 1
    print(f"ownership: {len(files)} file(s), {violations} violation(s), {warnings} warning(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
