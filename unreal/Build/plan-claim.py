#!/usr/bin/env python3
"""Claim, renew, release or update a workstream file (PROGRAMME.md Section 6.2).

Edits only the named workstream file. Push the resulting one-file commit directly to
unreal/main after `git pull --rebase`; if the push is rejected, re-read the file — someone
else may have claimed it.

  plan-claim.py ws-04 --owner "session-abc" --branch ws/04-towers/rig       # claim (state -> claimed)
  plan-claim.py ws-04 --renew                                              # extend lease 24 h
  plan-claim.py ws-04 --state active --last-commit 1a2b3c4                 # update fields
  plan-claim.py ws-04 --release                                            # owner cleared, state -> paused
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plan_lib import STATES, WS_DIR, lease_expired, lease_iso, now_iso, parse_frontmatter, render_frontmatter  # noqa: E402


def find(ws: str) -> Path:
    ws = ws.lower().removeprefix("ws-")
    matches = sorted(WS_DIR.glob(f"ws-{ws}-*.md"))
    if not matches:
        sys.exit(f"no workstream file for ws-{ws} in {WS_DIR}")
    if len(matches) > 1:
        sys.exit(f"ambiguous: {[m.name for m in matches]}")
    return matches[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ws")
    ap.add_argument("--owner")
    ap.add_argument("--branch")
    ap.add_argument("--renew", action="store_true")
    ap.add_argument("--release", action="store_true")
    ap.add_argument("--state", choices=STATES)
    ap.add_argument("--last-commit")
    ap.add_argument("--blocked-on")
    ap.add_argument("--force", action="store_true", help="take over a workstream whose lease has not expired")
    a = ap.parse_args()

    path = find(a.ws)
    text = path.read_text(encoding="utf-8")
    meta, body = parse_frontmatter(text)
    if not meta:
        sys.exit(f"{path} has no frontmatter")

    if a.owner:
        current = meta.get("owner", "")
        if current and current != a.owner and not lease_expired(meta) and not a.force:
            sys.exit(f"WS-{meta['ws']} is owned by {current!r} until {meta.get('lease_expires')}; use --force only after INT reassigns it")
        meta["owner"] = a.owner
        meta["claimed_at"] = now_iso()
        meta["lease_expires"] = lease_iso()
        if meta.get("state", "unclaimed") in ("unclaimed", "paused", "done"):
            meta["state"] = "claimed"
    if a.branch:
        meta["branch"] = a.branch
    if a.renew:
        meta["lease_expires"] = lease_iso()
    if a.state:
        meta["state"] = a.state
    if a.last_commit:
        meta["last_commit"] = a.last_commit
    if a.blocked_on is not None:
        meta["blocked_on"] = a.blocked_on
        if a.blocked_on and meta.get("state") == "active":
            meta["state"] = "blocked"
    if a.release:
        meta["owner"] = ""
        meta["lease_expires"] = ""
        meta["state"] = "paused" if meta.get("state") != "done" else "done"

    path.write_text(render_frontmatter(meta) + body, encoding="utf-8")
    print(f"updated {path.name}: state={meta.get('state')} owner={meta.get('owner') or '—'} lease={meta.get('lease_expires') or '—'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
