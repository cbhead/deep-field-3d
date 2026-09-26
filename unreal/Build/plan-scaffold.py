#!/usr/bin/env python3
"""Create any workstream file listed in unreal/PLAN/registry.json that does not exist yet.

Never overwrites: workstream files are edited by their owning sessions after creation.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from plan_lib import WS_DIR, load_registry, render_frontmatter, ws_filename  # noqa: E402

TEMPLATE = """{frontmatter}
# WS-{ws} — {title}

## Scope / DoD
**Scope.** {scope}

**Definition of done.** {dod}

**Spec.** {spec} (in `unreal/PLAN/PROGRAMME.md`). Size {size}, phase {phase}.

## Contracts I consume
{consumes}

## Contracts / interfaces I provide
{provides}

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
"""


def main() -> int:
    WS_DIR.mkdir(parents=True, exist_ok=True)
    created = 0
    for e in load_registry():
        path = ws_filename(e)
        if path.exists():
            continue
        meta = {
            "ws": e["ws"],
            "slug": e["slug"],
            "title": e["title"],
            "state": "unclaimed",
            "owner": "",
            "claimed_at": "",
            "lease_expires": "",
            "branch": "",
            "last_commit": "",
            "editor_heavy": "true" if e.get("editor_heavy") else "false",
            "phase": e.get("phase", ""),
            "size": e.get("size", ""),
            "critical": "true" if e.get("critical") else "false",
            "blocked_on": "",
        }
        consumes = "\n".join(f"- {c}" for c in e.get("consumes", [])) or "- (none)"
        provides = "\n".join(f"- {c}" for c in e.get("provides", [])) or "- (none)"
        path.write_text(
            TEMPLATE.format(
                frontmatter=render_frontmatter(meta).rstrip("\n"),
                ws=e["ws"],
                title=e["title"],
                scope=e["scope"],
                dod=e["dod"],
                spec=e["spec"],
                size=e.get("size", ""),
                phase=e.get("phase", ""),
                consumes=consumes,
                provides=provides,
            ),
            encoding="utf-8",
        )
        created += 1
    print(f"created {created} workstream file(s) in {WS_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
