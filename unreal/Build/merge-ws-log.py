#!/usr/bin/env python3
"""Union-merge a workstream ledger file that conflicted in a rebase (PLAN/README.md, "INT: do not
write into a workstream file while its PR is open").

A workstream `.md` is edited by its own branch AND pushed to directly — by its owner renewing a §6.2
lease, and by INT recording a ruling. When both happen the open PR conflicts, and the resolution is
always the same: the dated sections are append-only, so both sides are real history and the answer is
a union in date order, never a winner. The frontmatter is a snapshot: ours (the side being rebased
onto) is kept, and the branch's frontmatter lines it drops are printed.

Dated entries the branch added under any other heading (INT sometimes inserts a section, e.g. "## From
INT (…)", above which later log entries then keep being appended) are inserted after the last dated
entry of the same section on our side; the section's prose is left alone. And before writing, every
line the branch added relative to the merge base must be in the result, frontmatter aside: if one is
not, nothing is written and the exit is 1, so a landing stops for a human instead of dropping it.
(2026-09-26: before this check, a branch's entry under "## From INT" was silently dropped, the rebased
commit became empty, and int-merge skipped it.)

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


def stage(path: str, n: int, required: bool = True) -> str:
    out = subprocess.run(["git", "show", f":{n}:{path}"], capture_output=True, text=True, encoding="utf-8")
    if out.returncode != 0:
        if not required:
            return ""          # stage 1 is absent when both sides added the file
        raise SystemExit(f"merge-ws-log: {path} has no stage {n} — is it actually conflicted?")
    return out.stdout


def norm(e: str) -> str:
    return re.sub(r"\s+", " ", e).strip()[:150]


def frontmatter_end(lines: list[str]) -> int:
    """Index just past the closing `---` of the frontmatter, or 0 when there is none."""
    if lines and lines[0].strip() == "---":
        for k in range(1, len(lines)):
            if lines[k].strip() == "---":
                return k + 1
    return 0


def dated_entries_by_section(lines: list[str]):
    """[(section header or '', entry lines)] for every dated entry after the frontmatter."""
    out, header, cur = [], "", None
    for l in lines[frontmatter_end(lines):]:
        if l.startswith("## "):
            header, cur = l.strip(), None
        elif DATED.match(l):
            cur = [l]; out.append((header, cur))
        elif cur is not None and l.strip() and (l.startswith("  ") or l.startswith("\t")):
            cur.append(l)
        elif not l.strip():
            cur = None
    return out


def insert_into_section(merged: list[str], header: str, entry: list[str]) -> bool:
    """Put an entry after the last dated entry of `header`'s section (or at its end); False if absent."""
    start = frontmatter_end(merged)
    if header:
        try:
            start = next(k for k in range(start, len(merged)) if merged[k].strip() == header)
        except StopIteration:
            return False
    end = next((k for k in range(start + 1, len(merged)) if merged[k].startswith("## ")), len(merged))
    at, k = None, start + 1
    while k < end:
        if DATED.match(merged[k]):
            k += 1
            while k < end and merged[k].strip() and (merged[k].startswith("  ") or merged[k].startswith("\t")):
                k += 1
            at = k
        else:
            k += 1
    if at is None:   # no dated entry yet: before the section's trailing blank lines
        at = end
        while at > start + 1 and not merged[at - 1].strip():
            at -= 1
    merged[at:at] = entry
    return True


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
    # A Windows console's code page cannot print every character a ledger holds; never crash on one.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="replace")
    if len(sys.argv) < 2:
        print(__doc__); return 2
    path = sys.argv[1]
    check_only = "--check" in sys.argv
    ours, theirs = stage(path, 2).splitlines(), stage(path, 3).splitlines()
    base = stage(path, 1, required=False).splitlines()
    merged = list(ours)          # ours = the side already on the branch being rebased onto
    did = []
    for header in UNION_SECTIONS:
        a, b = entries(ours, header), entries(theirs, header)
        if a is None:
            continue
        oi, oj, oe = a
        te = b[2] if b else []
        have = {norm(e) for e in oe}
        added = [e for e in te if norm(e) not in have]
        all_entries = sorted(oe + added, key=lambda e: DATED.match(e).group(1))
        keep = [l for l in ours[oi + 1:oj] if l.strip().startswith("<!--")]
        tail = [""] if oj > oi + 1 and not ours[oj - 1].strip() else []
        merged[oi + 1:oj] = keep + all_entries + tail
        did.append(f"{header}: {len(oe)} + {len(added)} new = {len(all_entries)}")
    merged = "\n".join(merged).split("\n")   # the union above stores multi-line entries as one item

    # The branch's dated entries under any other heading.
    have = {norm("\n".join(e)) for _, e in dated_entries_by_section(merged)}
    moved = 0
    for header, entry in dated_entries_by_section(theirs):
        if header in UNION_SECTIONS or norm("\n".join(entry)) in have:
            continue
        if not insert_into_section(merged, header, entry):
            print(f"merge-ws-log: {path}: the branch added an entry under '{header}', which our side does not have — resolve by hand")
            return 1
        have.add(norm("\n".join(entry))); moved += 1
    if moved:
        did.append(f"{moved} entr{'y' if moved == 1 else 'ies'} kept under other headings")

    # Never lose a line: everything the branch added (relative to the base) must be in the result.
    fm_t, fm_m = frontmatter_end(theirs), frontmatter_end(merged)
    base_set = {norm(l) for l in base}
    result_set = {norm(l) for l in merged[fm_m:]}
    lost = [l for l in theirs[fm_t:] if l.strip() and norm(l) not in base_set and norm(l) not in result_set]
    if lost:
        print(f"merge-ws-log: {path}: the union would drop {len(lost)} line(s) the branch added — resolve by hand:")
        for l in lost[:10]:
            print(f"    {l[:160]}")
        return 1
    fm_dropped = [l for l in theirs[:fm_t] if l.strip() != "---" and l not in merged[:fm_m] and l not in base]
    if fm_dropped:
        did.append("frontmatter: ours kept, the branch's " + ", ".join(l.split(":")[0] for l in fm_dropped) + " dropped")
    text = "\n".join(merged).rstrip("\n") + "\n"
    if re.search(r"^(<<<<<<<|=======|>>>>>>>)", text, re.M):
        print(f"merge-ws-log: {path} still has conflict markers after the union — resolve by hand")
        return 1
    if check_only:
        print(f"merge-ws-log: {path} is mergeable ({'; '.join(did) or 'frontmatter only'})")
        return 0
    # UTF-8 and LF on every platform: Windows' defaults (cp1252, CRLF) would rewrite every line.
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    subprocess.run(["git", "add", path], check=True)
    print(f"merge-ws-log: {path} union-merged and staged — {'; '.join(did) or 'frontmatter only'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
