#!/usr/bin/env python3
"""Every registered DF.* test suite must be in the gate filter (CONTRACTS/ci.md).

The landing gate ran `DF.Unit+DF.Content` for weeks while DF.Online, DF.Editor, DF.UI and DF.Func
existed — 15 of 87 registered tests gating nothing, each written, run once by its author, and never
run again. Nothing detected it because a filter that omits a suite is indistinguishable from a suite
that does not exist. This makes the omission fail a check.

  python3 unreal/Build/check-test-coverage.py            # exit 1 if a suite is outside the gate
  python3 unreal/Build/check-test-coverage.py --list      # print the roots it found and where

The gate filter is test.sh's own default (DF_GATE_FILTER), so there is exactly one definition and
int-merge, ci-local and pr-check all inherit it.
"""
from __future__ import annotations
import re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "unreal" / "DeepField" / "Source"
TEST_SH = Path(__file__).resolve().parent / "test.sh"
# IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "DF.Unit.Thing", flags) and friends.
REG = re.compile(r'IMPLEMENT_[A-Z_]*TEST\s*\([^,]+,\s*"(DF\.[^"]+)"')
# Suites deliberately outside the landing gate, each with the reason it cannot run there.
# An entry here is a decision; an omission from the gate without an entry is an accident.
EXCLUDED = {
    "DF.Perf": "needs the GPU box and a frame budget; nightly there, never on the Mac",
    "DF.Soak": "hours long by design; Gauntlet lane only",
}


def gate_filter() -> str:
    m = re.search(r'^DF_GATE_FILTER="([^"]+)"', TEST_SH.read_text(), re.M)
    if not m:
        print(f"check-test-coverage: no DF_GATE_FILTER= in {TEST_SH.name}")
        raise SystemExit(2)
    return m.group(1)


def roots() -> dict[str, list[str]]:
    found: dict[str, list[str]] = {}
    for f in SOURCE.rglob("*.cpp"):
        for name in REG.findall(f.read_text(errors="replace")):
            root = ".".join(name.split(".")[:2])          # DF.Unit.Status.Foo -> DF.Unit
            found.setdefault(root, []).append(f"{f.relative_to(REPO)}: {name}")
    return found


def main() -> int:
    gate = set(gate_filter().split("+"))
    found = roots()
    if "--list" in sys.argv:
        for root in sorted(found):
            where = "in the gate" if root in gate else ("excluded: " + EXCLUDED[root] if root in EXCLUDED else "NOT GATED")
            print(f"  {root:14} {len(found[root]):3} test(s)  {where}")
        return 0
    missing = sorted(r for r in found if r not in gate and r not in EXCLUDED)
    stale = sorted(g for g in gate if g not in found)
    for root in missing:
        print(f"NOT GATED  {root}  ({len(found[root])} test(s)) — add it to DF_GATE_FILTER in test.sh,")
        print(f"           or to EXCLUDED here with the reason it cannot run in a landing. First: {found[root][0]}")
    for g in stale:
        print(f"note       {g} is in the gate but no test registers it (a suite not written yet, or a typo)")
    print(f"test coverage: {len(found)} suite(s) registered, {len(gate)} gated, {len(missing)} ungated")
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
