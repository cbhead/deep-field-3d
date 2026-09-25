---
ws: 15
slug: automation-ci
title: Automation, CI, packaging, store
state: paused
owner: 
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 
branch: ws/15-automation-ci/harness
last_commit: bd9d16a
editor_heavy: false
phase: P1+
size: M
critical: false
blocked_on: 
---
# WS-15 — Automation, CI, packaging, store

## Scope / DoD
**Scope.** Test base classes, Gauntlet controllers, unreal/Build scripts (layering, ownership, LFS lock audit, plan-status), GitHub Actions (self-hosted GPU-box nightly and render lanes; Windows ports of the unreal/Build zsh scripts, ADR-0028), Windows packaging, EGS BuildPatchTool, crash reporting.

**Definition of done.** Nightly build + all DF.* on the GPU box; packaged Windows builds to the EGS dev sandbox.

**Spec.** §7 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P1+.

## Contracts I consume
- C1
- C15 (DF.Unit.Tags.MessageInventoryHasTagPerEntry walks DFMessageList.inl)

## Contracts / interfaces I provide
- test base classes
- CI

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-19 · contract-append (no RFC): `Source/DFCore/Public/Testing/DFTestUtils.h` — header-only `FDFTestWorld` / `FDFMessageCapture` for every module's `Private/Tests` (WS-00's module; no `.Build.cs` change). Dependents: none yet.
- 2026-09-19 · contract-append: `PLAN/CONTRACTS/ci.md` (new: lanes, self-hosted runner, test naming) + one row in `PLAN/CONTRACTS/README.md`.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- Land `PLAN/CONTRACTS/ci.md` and the `CONTRACTS/README.md` row (contract-appends into INT-owned `PLAN/**`).
- Accept `Source/DFCore/Public/Testing/DFTestUtils.h` as a contract-append into WS-00's DFCore (header-only).
- Copy the Unreal workflow files (`unreal-checks.yml`, and the Windows lane once it exists) to `main` (the default branch), and remove the retired `unreal-mac.yml` from both branches: GitHub fires `schedule` / lists `workflow_dispatch` only from there (ci.md §0). Repeat when they change.
- Human: register the GPU box as the self-hosted runner (ci.md, "The GPU box as the self-hosted runner"); repository variables `UE_ROOT` and optionally `CI_LOCAL_ARGS`; `UE-LocalDataCachePath` in the runner's `.env`.
- Regenerate `STATUS.md` at landing (`int-merge.sh` does): `ci-local.sh` reports it stale (WARN) on today's `unreal/main` (WS-12 claim, WS-01 renewal).
- `unreal-checks.yml` runs `dotnet run --project tools/content-export -- --diff` on ubuntu (dotnet 8): WS-01's exporter and the `sim/` tree must build there; drift on `unreal/main` makes that lane red until WS-01 re-exports (ADR-0005 intent).

## Open questions
- Should a stale `STATUS.md` make the nightly red? `ci-local.sh` warns by default (claims and lease renewals land on `unreal/main` between INT cycles by design, §6.2); INT flips it with `CI_LOCAL_ARGS=--strict`.
- The GPU-box PR lane (planned) builds but runs no tests: the box is shared with the agent sessions, and a PR's own tests run there by hand (`unreal/README.md` §5.4). Revisit once the lane exists and its cost is measured.
- UBT's `-WaitMutex` queue: seven agents' builds queued behind one full build today (a no-change incremental took 1892 s wall). Worth a protocol note — stagger builds, or a queued build lock like `editor-lock.sh`.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b/agent-ws15 · landed: `DFTestUtils.h` (FDFTestWorld, FDFMessageCapture); DF.Unit.MessageBus.{ParentFanout,UnsubscribeDuringDelivery} + DF.Unit.Tags.{ContentIdMapping,MessageInventoryHasTagPerEntry} (4/4 pass through editor-lock.sh test.sh); test.sh verdict from the JSON report (proved: a failing DF.Dev test → exit 1, an empty filter → exit 1 / 0 with DF_TEST_ALLOW_EMPTY=1; the proof test deleted, rebuilt); editor-lock.sh no-exec fix (also on main as 9fc6e61); smoke-listen.sh -client-count N (host + 2 clients OK); ci-local.sh end-to-end on the Mac (509 s: layering/ownership/schema ok, plan-status WARN, build 382 s, tests 43 s, smoke 82 s); pr-check.sh; Build/README.md documents every script incl. int-merge.sh; .github/workflows/unreal-checks.yml + unreal-mac.yml (PyYAML-valid; the nightly checks out unreal/main explicitly, the default branch being main); CONTRACTS/ci.md. Next: Gauntlet controllers (Plugins/DFAutomation), DF.Net.ListenHostPlusClient as a real test, Mac packaging (nightly-only, both editor slots), LFS lock audit, crash reporting. Needs INT: see above.

## From INT (2026-09-21) — a guard test that would have caught a whole silent subsystem
WS-02 found that `Config/Tags/DF_Gameplay.ini` had **never registered a single one of its 47 tags**: the
rows were written `+GameplayTagList=`, the combination form that belongs in the `DefaultGameplayTags.ini`
hierarchy, but a loose source under `Config/Tags/` is read by `SourceTagList->LoadConfig(...)` on that one
file (`GameplayTagsManager.cpp:582`), which wants the plain key. Every `GameplayCue.DF.*` leaf was an
invalid tag, `FireCue` returned early on `!Cue.IsValid()`, and **no status or reaction cue had ever
fired** — with green tests throughout, because nothing asserted the tags existed. `DF_Online.ini` had the
identical bug and INT fixed it on main the same day.

Please add `DF.Unit.Tags.EveryIniTagRegisters`: parse every `unreal/DeepField/Config/Tags/DF_*.ini`,
extract each `Tag="..."`, and assert `RequestGameplayTag(Name, /*ErrorIfNotFound*/ false).IsValid()` for
all of them. It is a few lines, it is O(files) forever, and it converts an entire class of silent
misconfiguration into a red test. Worth pairing with an assertion that every tag the native list
declares is also resolvable, so the two registration paths are both pinned.

- 2026-09-25 · INT · `DF.Unit.Tags.EveryIniTagRegisters` (your INT ask) and `test-gate-check.py` + `test-gate-exclusions.tsv` are on `claude/happy-babbage-t6qrhw` for review (written in a cloud session; the C++ is not built). **R14:** at the next INT cycle, one `editor-lock.sh test.sh DF` run, then each green suite moves into both landing filters with its exclusion row deleted (ci.md, 2026-09-25).
- 2026-09-25 · INT · Correction to the note above: R14 landed on main as `a6505ad` (the gate is defined once as `DF_GATE_FILTER` in `test.sh`, enforced by `check-test-coverage.py`, and all four suites were green and are gated). `test-gate-check.py` and its exclusion file were retired when PR #48 merged main; `ci-local.sh`'s `coverage` step and `unreal-checks.yml` now run `check-test-coverage.py`. `DF.Unit.Tags.EveryIniTagRegisters` is unaffected.
- 2026-09-25 · INT (cloud session, on the user's ask: "a single script that ensures all systems, programs, libraries exist before running") · **Windows one-script path** (PR from `claude/windows-setup`): `unreal/Build/deepfield.ps1` + `unreal/deepfield.cmd` (`setup | doctor | build | test [filter] | check | editor | play | host | join | solution`). `setup` checks and installs with winget (Git, Git LFS, Python 3.12, VS 2022 Community + the C++ game workloads, or updates/modifies an existing VS until it has an MSVC toolset `Windows_SDK.json` accepts), enables long paths, clones `unreal/main` with `core.autocrlf=false` and every LFS file, finds the engine the `.uproject` names through `LauncherInstalled.dat` / the registry (the engine install stays manual: the script opens the launcher and waits), sets `UE_ROOT`, builds. `test` reads `DF_GATE_FILTER` from `test.sh` and keeps its stale-binary refusal and JSON-report verdict. Windows PowerShell 5.1 compatible (PSScriptAnalyzer compatibility rules, 0 findings) and ASCII-only; **not yet run on Windows** — WS-15 owns `unreal/Build/**`, so please adopt it, and port `pr-check`/`ci-local` onto it rather than a second set of wrappers. `unreal/README.md` §0 and `windows-bringup.md` point at it.
