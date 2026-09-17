"""Shared helpers for the PLAN ledger scripts (plan-status, plan-scaffold, plan-claim).

Workstream files carry a small YAML-ish frontmatter between `---` lines. We parse it
ourselves (key: value per line) so the scripts need nothing beyond the standard library.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PLAN = REPO / "unreal" / "PLAN"
WS_DIR = PLAN / "workstreams"
REGISTRY = PLAN / "registry.json"
STATUS = PLAN / "STATUS.md"

FRONTMATTER_RE = re.compile(r"^---\n(.*?)\n---\n", re.S)
STATES = ("unclaimed", "claimed", "active", "blocked", "review", "done", "paused")


@dataclass
class Workstream:
    path: Path
    meta: dict[str, str]
    body: str = field(default="")

    @property
    def ws(self) -> str:
        return self.meta.get("ws", "")

    @property
    def slug(self) -> str:
        return self.meta.get("slug", "")


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def lease_iso(hours: int = 24) -> str:
    return (datetime.now(timezone.utc) + timedelta(hours=hours)).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_frontmatter(text: str) -> tuple[dict[str, str], str]:
    m = FRONTMATTER_RE.match(text)
    if not m:
        return {}, text
    meta: dict[str, str] = {}
    for line in m.group(1).splitlines():
        if ":" not in line or line.lstrip().startswith("#"):
            continue
        key, _, value = line.partition(":")
        value = value.split("#", 1)[0].strip()
        meta[key.strip()] = value
    return meta, text[m.end():]


def render_frontmatter(meta: dict[str, str]) -> str:
    keys = ["ws", "slug", "title", "state", "owner", "claimed_at", "lease_expires", "branch", "last_commit", "editor_heavy", "phase", "size", "critical", "blocked_on"]
    lines = ["---"]
    for k in keys:
        if k in meta:
            lines.append(f"{k}: {meta[k]}")
    for k, v in meta.items():
        if k not in keys:
            lines.append(f"{k}: {v}")
    lines.append("---")
    return "\n".join(lines) + "\n"


def load_workstreams() -> list[Workstream]:
    out: list[Workstream] = []
    for p in sorted(WS_DIR.glob("ws-*.md")):
        meta, body = parse_frontmatter(p.read_text(encoding="utf-8"))
        out.append(Workstream(p, meta, body))
    return out


def load_registry() -> list[dict]:
    return json.loads(REGISTRY.read_text(encoding="utf-8"))["workstreams"]


def ws_filename(entry: dict) -> Path:
    return WS_DIR / f"ws-{entry['ws']}-{entry['slug']}.md"


def lease_expired(meta: dict[str, str]) -> bool:
    exp = meta.get("lease_expires", "")
    if not exp:
        return False
    try:
        dt = datetime.fromisoformat(exp.replace("Z", "+00:00"))
    except ValueError:
        return False
    return dt < datetime.now(timezone.utc)
