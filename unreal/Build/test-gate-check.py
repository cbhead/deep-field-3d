#!/usr/bin/env python3
"""Every registered DF.* automation test is either in the landing filter or excluded with a reason.

CONTRACTS/ci.md (INT, 2026-09-21): `int-merge` and `ci-local` ran `DF.Unit+DF.Content` while four
other suites grew beside them, so 15 of 87 registered tests had never gated a landing and nothing
said so. This check makes that state a decision on paper instead of an accident:

  * the registrations are read from the source (`IMPLEMENT_*AUTOMATION_TEST(Class, "Name", ...)` and
    `BEGIN_DEFINE_SPEC(Class, "Name", ...)`), never from anyone's memory of them;
  * the landing filter is read from `int-merge.sh` and `ci-local.sh`, and the two must agree;
  * `test-gate-exclusions.tsv` names every prefix the landing does NOT run, with an owner and a reason.

Fails when a registered test is neither gated nor excluded, when an exclusion has no reason or matches
no test (stale), or when the two scripts' filters differ. No engine; seconds.

    python3 unreal/Build/test-gate-check.py            # the check (ci-local.sh step `test-gate`)
    python3 unreal/Build/test-gate-check.py --list     # also print every test and its verdict

Matching follows `Automation RunTests <A>+<B>`: a test runs when its full name starts with one of the
'+'-joined filter parts (all DF names are dotted paths, so a prefix is the only match that happens).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PROJECT = REPO / "unreal" / "DeepField"
BUILD = REPO / "unreal" / "Build"
EXCLUSIONS = BUILD / "test-gate-exclusions.tsv"
SOURCE_DIRS = [PROJECT / "Source", PROJECT / "Plugins"]
SOURCE_EXT = {".cpp", ".h", ".inl"}

REGISTRATION = re.compile(
    r"\b(?:IMPLEMENT_\w*AUTOMATION_TEST\w*|BEGIN_DEFINE_SPEC\w*|DEFINE_SPEC\w*)\s*\(\s*\w+\s*,\s*\"([^\"]+)\"",
    re.MULTILINE,
)
CI_LOCAL_FILTER = re.compile(r'^FILTER="([^"]+)"', re.MULTILINE)
INT_MERGE_FILTER = re.compile(r"test\.sh\s+(DF[\w.+]+)")


def registered_tests() -> dict[str, Path]:
    tests: dict[str, Path] = {}
    for root in SOURCE_DIRS:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix not in SOURCE_EXT or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for name in REGISTRATION.findall(text):
                if name.startswith("DF."):
                    tests.setdefault(name, path.relative_to(REPO))
    return tests


def landing_filter(errors: list[str]) -> list[str]:
    ci_local = CI_LOCAL_FILTER.findall((BUILD / "ci-local.sh").read_text())
    int_merge = INT_MERGE_FILTER.findall((BUILD / "int-merge.sh").read_text())
    if len(ci_local) != 1:
        errors.append(f"ci-local.sh: expected one FILTER=\"...\" default, found {len(ci_local)}")
    if len(int_merge) != 1:
        errors.append(f"int-merge.sh: expected one `test.sh <filter>` call, found {len(int_merge)}")
    if not ci_local or not int_merge:
        return []
    parts = lambda f: sorted(p for p in f.split("+") if p)
    if parts(ci_local[0]) != parts(int_merge[0]):
        errors.append(f"the landing filters disagree: ci-local.sh runs {ci_local[0]}, int-merge.sh runs {int_merge[0]}")
    return parts(int_merge[0])


def load_exclusions(errors: list[str]) -> list[tuple[str, str, str]]:
    rows: list[tuple[str, str, str]] = []
    if not EXCLUSIONS.exists():
        return rows
    for n, line in enumerate(EXCLUSIONS.read_text().splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        cols = [c.strip() for c in line.split("\t")]
        if len(cols) < 3 or not cols[0] or not cols[1] or not cols[2]:
            errors.append(f"{EXCLUSIONS.name}:{n}: needs prefix<TAB>owner<TAB>reason — an exclusion without a reason is an accident, not a decision")
            continue
        rows.append((cols[0], cols[1], cols[2]))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--list", action="store_true", help="print every registered test and its verdict")
    a = ap.parse_args()

    errors: list[str] = []
    warnings: list[str] = []
    tests = registered_tests()
    gate = landing_filter(errors)
    exclusions = load_exclusions(errors)
    if not tests:
        errors.append("no DF.* test registrations found — the scan is broken, not the suite")

    ungated: list[str] = []
    suites: dict[str, list[int]] = {}   # suite -> [registered, gated, excluded]
    used = {prefix: 0 for prefix, _, _ in exclusions}
    for name in sorted(tests):
        suite = ".".join(name.split(".")[:2])
        row = suites.setdefault(suite, [0, 0, 0])
        row[0] += 1
        gated = any(name.startswith(p) for p in gate)
        excluded_by = [p for p, _, _ in exclusions if name.startswith(p)]
        for p in excluded_by:
            used[p] += 1
        if gated:
            row[1] += 1
            if excluded_by:
                warnings.append(f"{name} is gated AND excluded by {', '.join(excluded_by)} — drop the exclusion")
        elif excluded_by:
            row[2] += 1
        else:
            ungated.append(name)
        if a.list:
            verdict = "gated" if gated else (f"excluded ({excluded_by[0]})" if excluded_by else "UNGATED")
            print(f"  {verdict:<28} {name}  [{tests[name]}]")

    for prefix, owner, _ in exclusions:
        if used[prefix] == 0:
            errors.append(f"exclusion {prefix} ({owner}) matches no registered test — stale, remove it")
    for name in ungated:
        errors.append(f"{name} ({tests[name]}) is in no landing filter and no exclusion — gate it, or exclude it with a reason in {EXCLUSIONS.name}")

    print(f"landing filter: {'+'.join(gate) or '(unreadable)'}")
    print(f"  {'suite':<12} {'tests':>5} {'gated':>6} {'excluded':>9}")
    for suite, (n, g, x) in sorted(suites.items()):
        print(f"  {suite:<12} {n:>5} {g:>6} {x:>9}")
    total = sum(r[0] for r in suites.values())
    gated_total = sum(r[1] for r in suites.values())
    print(f"  {'total':<12} {total:>5} {gated_total:>6} {sum(r[2] for r in suites.values()):>9}")
    for prefix, owner, reason in exclusions:
        print(f"  excluded {prefix} ({owner}, {used[prefix]} test(s)): {reason}")
    for w in warnings:
        print(f"warning: {w}")
    for e in errors:
        print(f"ERROR: {e}")
    print(f"test-gate: {total} registered, {gated_total} gate a landing, {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
