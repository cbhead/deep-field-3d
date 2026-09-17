#!/usr/bin/env python3
"""Regenerate unreal/PLAN/STATUS.md from the workstream files' frontmatter.

Run by INT each cycle (and by anyone who wants a current view). Never hand-edit STATUS.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plan_lib import STATUS, load_workstreams, lease_expired, now_iso  # noqa: E402


def main() -> int:
    rows = []
    counts: dict[str, int] = {}
    for w in load_workstreams():
        m = w.meta
        state = m.get("state", "unclaimed")
        if state not in ("done",) and m.get("owner", "") and lease_expired(m):
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
    text = (
        "# Workstream status\n\n"
        f"Generated {now_iso()} by `unreal/Build/plan-status.py` from `workstreams/*.md`. Do not edit by hand.\n\n"
        f"{summary}\n\n"
        "| WS | Title | Phase | CP | State | Owner | Lease expires | Last commit | Editor-heavy | Blocked on |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n" + "\n".join(rows) + "\n"
    )
    STATUS.write_text(text, encoding="utf-8")
    print(f"wrote {STATUS} ({len(rows)} workstreams)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
