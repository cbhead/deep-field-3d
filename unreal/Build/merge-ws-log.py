#!/usr/bin/env python3
"""Union-merge a workstream ledger file that conflicted in a rebase (PLAN/README.md, "INT: do not
write into a workstream file while its PR is open").

A workstream `.md` is edited by its own branch AND pushed to directly — by its owner renewing a §6.2
lease, and by INT recording a ruling. When both happen the open PR conflicts, and the resolution is
always the same: the dated sections are append-only, so both sides are real history and the answer is
a union in date order, never a winner. The frontmatter is a snapshot, so the newer side wins there.

    python3 unreal/Build/merge-ws-log.py <path>          # read ours/theirs from the git index
    python3 unreal/Build/merge-ws-log.py <path> --check   # exit 1 if it cannot merge, change nothing

Run from inside the worktree holding the conflict. Writes the merged file and stages it. Prints what
it did, because a ledger merged silently is a ledger nobody audits.
"""
from __future__ import annotations
import re, subprocess, sys
from pathlib import Path

DATED = re.compile(r"^- (\d{4}-\d{2}-\d{2})")
# Sections whose entries are append-only history: union them. Anything else takes the newer side.
UNION_SECTIONS = ("## Session log", "## Interfaces I changed")


def stage(path: str, n: int) -> str:
    out = subprocess.run(["git", "show", f":{n}:{path}"], capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit(f"merge-ws-log: {path} has no stage {n} — is it actually conflicted?")
    return out.stdout


def entries(lines: list[str], header: str):
    """(start, end, [entry]) for a section; an entry is its `- YYYY-MM-DD` line plus indented lines."""
    try:
        i = next(k for k, l in enumerate(lines) if l.strip() == header)
    except StopIteration:
        return None
    j = next((k for k in range(i + 1, len(lines)) if lines[k].startswith("## ")), len(lines))
    out, cur = [], None
    for l in lines[i + 1:j]:
        if DATED.match(l):
            cur = [l]; out.append(cur)
        elif cur is not None and l.strip() and (l.startswith("  ") or l.startswith("\t")):
            cur.append(l)
        elif not l.strip():
            cur = None
    return i, j, ["\n".join(e) for e in out]


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__); return 2
    path = sys.argv[1]
    check_only = "--check" in sys.argv
    ours, theirs = stage(path, 2).splitlines(), stage(path, 3).splitlines()
    merged = list(ours)          # ours = the side already on the branch being rebased onto
    did = []
    for header in UNION_SECTIONS:
        a, b = entries(ours, header), entries(theirs, header)
        if a is None:
            continue
        oi, oj, oe = a
        te = b[2] if b else []
        norm = lambda e: re.sub(r"\s+", " ", e).strip()[:150]
        have = {norm(e) for e in oe}
        added = [e for e in te if norm(e) not in have]
        all_entries = sorted(oe + added, key=lambda e: DATED.match(e).group(1))
        keep = [l for l in ours[oi + 1:oj] if l.strip().startswith("<!--")]
        tail = [""] if oj > oi + 1 and not ours[oj - 1].strip() else []
        merged[oi + 1:oj] = keep + all_entries + tail
        did.append(f"{header}: {len(oe)} + {len(added)} new = {len(all_entries)}")
    text = "\n".join(merged).rstrip("\n") + "\n"
    if re.search(r"^(<<<<<<<|=======|>>>>>>>)", text, re.M):
        print(f"merge-ws-log: {path} still has conflict markers after the union — resolve by hand")
        return 1
    if check_only:
        print(f"merge-ws-log: {path} is mergeable ({'; '.join(did) or 'frontmatter only'})")
        return 0
    Path(path).write_text(text)
    subprocess.run(["git", "add", path], check=True)
    print(f"merge-ws-log: {path} union-merged and staged — {'; '.join(did) or 'frontmatter only'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
