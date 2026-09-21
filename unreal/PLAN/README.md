# Deep Field 3D on Unreal — programme ledger

This directory is the coordination surface for every session working on the Unreal rebuild.
The programme itself — decisions, contracts, workstreams, phases, gameplay and art specs — is
[PROGRAMME.md](PROGRAMME.md). Read it before anything else; Sections 3, 5 and 6 are mandatory
for every workstream, the Appendices for the workstreams that cite them.

| File / dir | What it is | Who writes it |
|---|---|---|
| `PROGRAMME.md` | The plan of record (vendored from `~/.claude/plans/`). Changes land as PRs like any other file. | INT |
| `STATUS.md` | One table of every workstream: state, owner, lease, last commit, blocked-on, next. **Generated** by `unreal/Build/plan-status.py` — never hand-edited. | script |
| `OWNERSHIP.md` | Path glob → workstream. CI refuses a PR that touches binaries outside the PR's workstream globs. | INT |
| `DECISIONS.md` | ADR log. Append-only; INT numbers new entries. | anyone appends |
| `CONTRACTS/` | One human-readable document per frozen contract C1–C16 plus `map-authoring-3d.md`. The twin of the header/asset it describes. | contract owner (RFC to change) |
| `rfcs/NNNN-<slug>.md` | Contract change proposals (Section 6.6). | author |
| `workstreams/ws-NN-<slug>.md` | One file per workstream: frontmatter (state/owner/lease/branch/last_commit/editor_heavy) + Scope/DoD, contracts consumed, interfaces changed, open questions, session log. | the owning session |
| `EDITOR-SLOTS.md` | The two editor-heavy slots on the 8 GB Mac (Section 6.7). | sessions taking/releasing a slot |
| `digests/YYYY-MM-DD.md` | INT cycle digest: merged PRs, interface changes, expired leases, red tests, open RFCs, unverified render lanes. | INT |
| `registry.json` | The workstream registry used by `plan-scaffold.py` to create missing ws files. Edit only to add a workstream (INT). | INT |

## Working copies (ADR-0021)

- **Unreal working copy:** `/Volumes/Toshiba/Deepfield-Unreal/deepfield-3d` on the external SSD — open the editor, build, cook and import only here. DDC: `/Volumes/Toshiba/Deepfield-Unreal/DDC`.
- Parallel sessions: `git worktree add /Volumes/Toshiba/Deepfield-Unreal/wt-ws-NN -b ws/NN-<slug>/<topic> origin/unreal/main` from that clone.
- The internal-disk checkout (`~/dev/deepfield-3d`) has ~16 GB free: text-only work (ledger, JSON, docs, scripts).
- Build: `Engine/Build/BatchFiles/Mac/Build.sh DeepFieldEditor Mac Development -Project=<clone>/unreal/DeepField/DeepField.uproject`; tests: `unreal/Build/test.sh`; net smoke: `unreal/Build/smoke-listen.sh`.

## Session bootstrap prompt

Paste this into a new Claude Code session (replace `WS-NN`):

> You are working on Deep Field 3D's Unreal rebuild. Read `unreal/PLAN/PROGRAMME.md` (Sections 3, 5, 6 and the Appendix your workstream cites), then `unreal/PLAN/STATUS.md`, `unreal/PLAN/DECISIONS.md`, `unreal/PLAN/CONTRACTS/` for the contracts you consume, and the latest `unreal/PLAN/digests/`. Claim workstream **WS-NN** per Section 6.2 (or continue it if you own it), run the session-start checklist (6.5), take an editor slot if `editor_heavy` (6.7), then work toward its DoD in PRs of ≤1 day each to `unreal/main`. Never edit another workstream's paths; propose contract changes as RFCs (6.6). End with the session-end checklist.

## Claiming in one command

```bash
# from your worktree, on a branch off origin/unreal/main
python3 unreal/Build/plan-claim.py ws-04 --owner "<your session handle>" --branch ws/04-towers/rig
git pull --rebase origin unreal/main && git push origin HEAD:unreal/main   # the one allowed direct push (one file)
```

`plan-claim.py` edits only your workstream file (owner, `claimed_at`, `lease_expires` = +24 h, branch, state → `claimed`). If the push is rejected, re-read the file — someone else may have claimed it.

## Regenerating STATUS.md

```bash
python3 unreal/Build/plan-status.py            # writes unreal/PLAN/STATUS.md from workstream frontmatter
python3 unreal/Build/plan-scaffold.py          # creates any workstream file missing from registry.json (never overwrites)
```

## INT: do not write into a workstream file while its PR is open
Three ledger conflicts tonight had one cause. A workstream's `.md` is edited by its own branch *and*
pushed to directly — by its owner renewing a §6.2 lease, and by INT recording a ruling. The moment both
happen, the open PR conflicts, and the conflict is always the same shape: an append-only session log
where both sides are correct and the resolution is a union in date order, never a winner.

WS-05 drew the rule for itself first: **a direct ledger push is only safe while no open PR of yours
edits that file — otherwise rebase the PR immediately, not hours later.** INT then hit the same wall
from the other side, twice, by recording rulings in `ws-05-enemies-ai.md` and `ws-02-gameplay-core.md`
while both had PRs in flight.

So, for INT specifically:
- Prefer the **contract doc, the RFC, or the digest** for a ruling. Those are INT-owned and nobody's PR
  edits them. A ruling in `CONTRACTS/lanegraph.md` is also more findable later than one in a ws file.
- Write into a **workstream file** only when the note must be read by whoever claims it next (the
  `NotifyEnemyAdded` invariant in ws-17/19/20/24 is the right use — those are unclaimed), and check
  `gh pr list` first when the workstream is active.
- When a conflict happens anyway, the resolution is mechanical: union both sides' dated entries, sort
  by date, keep the newer frontmatter. Never take one side wholesale — both are real history.
