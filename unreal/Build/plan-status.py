#!/usr/bin/env python3
"""Regenerate unreal/PLAN/STATUS.md from the workstream files' frontmatter.

Run by INT each cycle (and by anyone who wants a current view). Never hand-edit STATUS.md.

  python3 unreal/Build/plan-status.py            # rewrite unreal/PLAN/STATUS.md
  python3 unreal/Build/plan-status.py --out F    # write the table to F instead (a temp file for diffing)
  python3 unreal/Build/plan-status.py --check    # exit 1 if the committed STATUS.md is stale (CI)

--check regenerates in memory at the committed file's own "Generated <time>" instant, so lease
expiry is judged as it was then and the only thing that can differ is the frontmatter: a claim,
a lease renewal, a last_commit or state change since INT last ran the generator.
"""
from __future__ import annotations

import argparse
import difflib
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plan_lib import STATUS, load_workstreams, lease_expired, now_iso  # noqa: E402

GENERATED_RE = re.compile(r"^Generated (\S+) by ", re.M)


def render(generated_at: str) -> str:
    try:
        now = datetime.fromisoformat(generated_at.replace("Z", "+00:00"))
    except ValueError:
        now = datetime.now(timezone.utc)
    rows = []
    counts: dict[str, int] = {}
    for w in load_workstreams():
        m = w.meta
        state = m.get("state", "unclaimed")
        if state not in ("done",) and m.get("owner", "") and lease_expired(m, now):
            state = f"{state} (lease expired)"
        counts[state.split(" ")[0]] = counts.get(state.split(" ")[0], 0) + 1
        rows.append(
            "| {ws} | {title} | {phase} | {crit} | {state} | {owner} | {lease} | {commit} | {editor} | {blocked} |".format(
                ws=f"WS-{m.get('ws','?')}",
                title=m.get("title", w.slug),
                phase=m.get("phase", ""),
                crit="CP" if m.get("critical", "false") == "true" else "",
                state=state,
                owner=m.get("owner", "") or "—",
                lease=m.get("lease_expires", "") or "—",
                commit=m.get("last_commit", "") or "—",
                editor="yes" if m.get("editor_heavy", "false") == "true" else "",
                blocked=m.get("blocked_on", "") or "—",
            )
        )
    summary = " · ".join(f"{k}: {v}" for k, v in sorted(counts.items()))
    return (
        "# Workstream status\n\n"
        f"Generated {generated_at} by `unreal/Build/plan-status.py` from `workstreams/*.md`. Do not edit by hand.\n\n"
        f"{summary}\n\n"
        "| WS | Title | Phase | CP | State | Owner | Lease expires | Last commit | Editor-heavy | Blocked on |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n" + "\n".join(rows) + "\n"
    ), len(rows)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", help="write here instead of unreal/PLAN/STATUS.md")
    ap.add_argument("--check", action="store_true", help="compare against the committed STATUS.md; exit 1 if stale")
    a = ap.parse_args()

    if a.check:
        if not STATUS.exists():
            print(f"plan-status: {STATUS} is missing — run plan-status.py")
            return 1
        committed = STATUS.read_text(encoding="utf-8")
        m = GENERATED_RE.search(committed)
        text, n = render(m.group(1) if m else now_iso())
        if text == committed:
            print(f"plan-status: OK ({n} workstreams, STATUS.md matches the frontmatter)")
            return 0
        diff = difflib.unified_diff(committed.splitlines(), text.splitlines(), "STATUS.md (committed)", "STATUS.md (regenerated)", lineterm="", n=0)
        for line in list(diff)[:60]:
            print(line)
        print("plan-status: STALE — run plan-status.py and commit unreal/PLAN/STATUS.md")
        return 1

    text, n = render(now_iso())
    target = Path(a.out) if a.out else STATUS
    target.write_text(text, encoding="utf-8")
    print(f"wrote {target} ({n} workstreams)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
