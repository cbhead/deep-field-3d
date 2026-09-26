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
- Copy the Unreal workflow files (`unreal-checks.yml`, and the Windows lane once it exists) to `main` (the default branch): GitHub fires `schedule` / lists `workflow_dispatch` only from there (ci.md §0). Repeat when they change.
- Human: register the GPU box as the self-hosted runner (ci.md, "The GPU box as the self-hosted runner"); repository variables `UE_ROOT` and optionally `CI_LOCAL_ARGS`; `UE-LocalDataCachePath` in the runner's `.env`.
- Regenerate `STATUS.md` at landing (`int-merge.sh` does): `ci-local.sh` reports it stale (WARN) on today's `unreal/main` (WS-12 claim, WS-01 renewal).
- `unreal-checks.yml` runs `dotnet run --project tools/content-export -- --diff` on ubuntu (dotnet 8): WS-01's exporter and the `sim/` tree must build there; drift on `unreal/main` makes that lane red until WS-01 re-exports (ADR-0005 intent).

## Open questions
- Should a stale `STATUS.md` make the nightly red? `ci-local.sh` warns by default (claims and lease renewals land on `unreal/main` between INT cycles by design, §6.2); INT flips it with `CI_LOCAL_ARGS=--strict`.
- The GPU-box PR lane (planned) builds but runs no tests: the box is shared with the agent sessions, and a PR's own tests run there by hand (`unreal/README.md` §5.4). Revisit once the lane exists and its cost is measured.
- UBT's `-WaitMutex` queue: seven agents' builds queued behind one full build today (a no-change incremental took 1892 s wall). Worth a protocol note — stagger builds, or a queued build lock like `editor-lock.sh`.

## Assignment from INT (2026-09-25) — audited, and the port is the point

**WS-15 is paused with no owner, and it is now the workstream with the most leverage in the programme.**
Not because CI is glamorous, but because of three facts the audit established:

1. **Zero self-hosted runners are registered**, so no lane has ever built or tested the Unreal tree.
   `unreal-checks` is green and compiles nothing; `unreal-win` exists and reports *skipped*.
2. **Four PRs have merged onto `unreal/main` through the GitHub button instead of the landing script**
   (`CONTRACTS/ci.md`). Two were explicitly unbuilt. One broke the trunk for an hour on a single symbol
   that does not exist in UE 5.8. The landing script refuses at the build step — it is the only thing
   that would have caught it, and it is the one script not yet ported.
3. **ADR-0028 retired the Mac**, so the six zsh scripts in `unreal/Build/` now run nowhere. Their rules
   are the accumulated scar tissue of fourteen defects found across two adversarial review rounds.

### The risk in this workstream is not "write PowerShell". It is losing the guards silently.
**This is demonstrated, not hypothetical.** The two ports already done (`pr-check`, `ci-local`/`smoke`)
each dropped something:
- `deepfield.ps1`'s staleness scan **omits the `.uproject`**, which `test.sh` includes — so a
  `.uproject` edit (a module added, a plugin enabled) can now be tested against a binary that predates it.
  That is precisely the failure the guard was written for: a green run that means nothing.
- **`DF_TEST_ALLOW_EMPTY` has no ps1 equivalent** while `unreal/Build/README.md:43` still documents it,
  so the "a filter that matches no test is a failure" rule has a documented escape hatch that does not exist.
Port the **rules**, not the scripts, and re-read each guard's comment for the reason it is there before
deciding it is incidental.

### `int-merge` is the last port and the one that matters
`deepfield.ps1`'s `ValidateSet` (line 61) has no `int-merge` verb and there is no `Invoke-IntMerge`.
`windows-bringup.md:95` already calls it the last WS-15 port. Its 259 lines are almost entirely guards;
these are the ones with a scar behind them, and what breaks if the port omits each:
- **A landing lock with crash detection** (60 s grace on a pid-less dir, reclaim only when the pid is
  dead) — without it two landings share one verify worktree and a checkout swaps the tree under a
  running test. This happened; the staleness guard caught it.
- **The verification record pins branch, base, HEAD, smoke and ws**, and the base is the **merge-base,
  not `origin/unreal/main`** — refs are shared by every worktree, so any session's fetch moves a ref
  mid-build. Recording the ref cost one landing three rejected pushes.
- **`--force-with-lease` armed with the branch tip the landing actually took**, persisted for `--resume`,
  not the ref a fetch refreshed a line earlier. Without this a landing silently overwrites an author who
  pushed during verification. It fired for real and refused, correctly.
- **`git rebase --continue` exits non-zero when it succeeds and stops at the next conflict.** Treating
  that as failure made a two-conflict branch unlandable. The loop must re-inspect and bail only when a
  step changes nothing at all.
- **Auto-resolution is limited to two globs** (`unreal/PLAN/STATUS.md`, regenerated; and
  `workstreams/ws-*.md`, union-merged in date order by `merge-ws-log.py`). Everything else stops for a
  human. Widening that set is how you lose someone's work to a merge you did not read.
- **Refuse to continue or skip while the worktree carries unstaged or untracked work** — `--skip` on a
  merely-unstaged resolution drops the author's commit and force-pushes the result.
- **Ledger/doc-only movement on the trunk re-runs the cheap checks and rebases without rebuilding**, but
  `OWNERSHIP.md` counts as one of those files and the ownership verdict depends on it, so the checks must
  run even on that path.
- **Yield to other builds before starting one**, counting only real UBT processes (`dotnet` running the
  dll) — `pgrep -f` alone matches every monitoring shell. On one machine with no 8 GB ceiling, decide
  deliberately whether this still earns its place and record the decision either way.
- **A build failure prints its diagnostics.** The filter was `" error "`, which matches
  "1 error generated." but not `Foo.cpp:12:34: error: …`; that cost two full rebuilds to recover a
  message the first build had already produced.
Two things have **no Windows analogue as written**: `stat -f %m` (use `(Get-Item).LastWriteTimeUtc`) and
the `/Volumes/Toshiba` worktree path.

### ADR-0028's follow-ups: what is actually left
Verified item by item. **Done:** `unreal-mac.yml` is gone from both `unreal/main` and `main` (`61d668f`);
`pr-check` and `ci-local`/`smoke` are ps1 verbs. **Outstanding:** `DeepField.uproject:50` still reads
`"TargetPlatforms": [ "Mac", "Windows" ]`; `.lfsconfig` still carries `fetchexclude` and
`deepfield.ps1` still has `-LightClone` (lines 86, 791-807); `check-test-coverage.py:27` still says
`DF.Perf` runs "never on the Mac"; `machine-inventory.ps1:326,347` still label readiness in Mac terms and
the generated `machines/windows-gpu.md:19,28` carries them; `int-merge` is unported.
**Two of those are INT's files, not yours** (`OWNERSHIP.md:8`: the `.uproject` and `.lfsconfig`) — send a
"Needs INT" line rather than editing them. **And ADR-0028 missed a sixth target:** §4.1's G1 still reads
"Mac CI builds `Development Editor`".

### Six places the docs overclaim — fix as you touch them
- **This file's `last_commit: bd9d16a` is on no branch**, and six later WS-15 landings are unrecorded in
  the session log. Reconstruct it from `git log` before you add to it.
- `OWNERSHIP.md:35` grants WS-15 `Plugins/DFAutomation/**` — **the directory does not exist.**
- Scope's **"LFS lock audit" has zero implementation** anywhere in `unreal/Build`.
- The DoD's second half — **"Windows packaging, EGS BuildPatchTool, crash reporting"** — exists only as
  plan text, with nothing in the tree.
- `PROGRAMME.md:356` calls `test.sh` "awaiting a Windows port" that has existed since 2026-09-25.
- `CONTRACTS/ci.md:213` says "Three incidents" above a **four-row table** — INT's own error, fixed in
  this commit.

### Order I would work it
1. **Register the runner** (`windows-bringup.md` §8) and make `unreal-win` actually run. Everything else
   in this workstream is worth less until a lane compiles the tree. Then tell the owner, because
   branch protection is theirs to enable and it is sequenced **after** the runner.
2. **Port `int-merge` onto `deepfield.ps1`**, guard by guard from the list above, and state in the PR
   which guard each block implements. Re-add the `.uproject` to the staleness scan and either implement
   `DF_TEST_ALLOW_EMPTY` or delete it from `README.md`.
3. The ADR-0028 leftovers, with the two INT files requested rather than edited.
4. The honest DoD gaps: LFS lock audit, Windows packaging, crash reporting — or renegotiate the DoD with
   INT if they belong to a later phase.

**G2 needs you** for "a packaged Windows build runs it", `DF.Net.Feel` and the 150 ms emulation; **G3**
for every ported probe green, four solo matches to victory in automation, and retiring Godot.

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
- 2026-09-25 · cloud session (on the owner's ask) · **`deepfield pr-check`**: `pr-check.sh` ported onto `deepfield.ps1`, and both now run the landing gate by default (`7f4a03e` made pr-check.sh inherit `DF_GATE_FILTER`). `deepfield pr-check [filter] [-Ws NN] [-Base REF]`: the repository checks + `ownership-check.py --ws NN` (workstream from the `ws/NN-slug/topic` branch, else `-Ws`), the build, the gate; a filter runs only that and reports `PARTIAL`, never `OK`. Puts an off-PATH Git on PATH for the ownership check (the box's Git is not on PATH per `machines/windows-gpu.md`). No smoke (its pass condition is under review). Checked with pwsh 7.4 in a cloud session: parses; PSScriptAnalyzer 5.1 compatibility rules 0 findings; the real functions driven with stubbed engine calls through six scenarios; **not yet run on Windows**. `ci-local`/`int-merge` are still to port.
- 2026-09-25 · cloud session (on the owner's ask) · **`deepfield smoke` and `deepfield ci-local`**: `smoke-listen.sh` and `ci-local.sh` ported onto `deepfield.ps1`, so only `int-merge` is left. The smoke's pass condition was re-derived against PR #48's join path (NEXT.md had flagged it): `player joined` is `PostLogin`, after `PreLogin`'s C14 validators, and a bare IP join to a `?listen` host is admitted by `UDFOnlineSubsystem`'s dev-join branch, so the count still means admitted. Now stricter, in both the port and `smoke-listen.sh`: every client must log `Welcomed by server` (the old check also accepted `Bringing up level for play took`, which a client that failed to connect logs when it falls back to its default map: a false positive), every seat > 0, and any `join refused` fails with its reason. `smoke`: port 7788 by default, `-Clients` 1–3 (four seats). `ci-local`: the same eight steps and WARN rule as ci-local.sh, `-Skip` (comma list), `-Strict`, `-Ws`/`-Base`, a summary table on every exit; a filter reports PARTIAL. The Windows workflow's step can be `unreal\deepfield ci-local`. Checked in a cloud session with pwsh 7.4 + PSScriptAnalyzer 1.23 (parses; 5.1 compatibility 0 findings) and by driving the real functions with scripted editor logs through 6 smoke and 8 ci-local scenarios; **not yet run on Windows**.
- 2026-09-25 · cloud session (on the owner's ask) · **`.github/workflows/unreal-win.yml`**, the Windows engine lane: `[self-hosted, Windows, X64, deepfield]`; `pull_request` to unreal/main runs `deepfield ci-local -Skip tests,smoke` (checks + build, per ci.md's PR-lane rule); the nightly (`cron 0 8 * * *`) and a dispatch (`full` ticked) run the whole `deepfield ci-local`; `CI_LOCAL_ARGS` now takes deepfield syntax (`-Strict`, `-Skip smoke`). Fork guard as before. **Armed by the repository variable `WIN_RUNNER_READY`**: until it is `true` the automatic triggers show as skipped instead of queuing for 24 h on every PR with no runner; a dispatch always runs. `clean: false` for incremental builds (a `clean` dispatch input forces a full one), with the previous run's logs and transcript deleted before checkout so an artifact or summary never shows another run's results. ci-local's summary table goes to the run page; logs + report + transcript to `unreal-win-logs-<run>`. Checked in a cloud session: actionlint 1.7.7 clean; the summary step (Windows PowerShell 5.1, 0 compatibility findings) driven through four transcript cases. **Not run yet: no runner.** To arm it: runner step 8 in ci.md (land the file on `main` too, dispatch once, set the variable).
- 2026-09-26 · cloud session with the owner at the GPU box · **First build and landing gate on MSVC: 155 of 155** (`e859ffb`; UE 5.8.2, MSVC 14.44.35207 folder / compiler 14.44.35229, Windows SDK 10.0.22621). `deepfield setup` first refused the toolset by its folder name (fixed in `a31f6f7`: judge by `cl.exe`'s build). The build then stopped in DFOnline on C2487 (`b234449`: one static per declaration in an exported class); every other module compiled clean. The first gate run failed `DF.Func.Status.ThermalShockInLevel` on the world subsystem's "no lane graph" error on `L_Dev_Empty` (`cb820b2`: `UDFWorldSubsystem::FindLaneGraph`, the quiet lookup `ADFMatchState` now uses), then crashed in `DF.Unit.LaneGraph.FoundryDerivation` on `Layers.FindChecked` (`e859ffb`: `Canon`'s `+ 0.0` signed-zero fold was optimised away, so equal positions hashed differently; it now folds on the bits). All 155 registered tests are in the gate and passed. Two new rows in the runbook's §8 cover pulling a crash callstack out of the test log and reading each module's `/fp:` flag. That flag is now in doubt for Editor targets (CONTRACTS/ci.md FP line, NEXT.md hazards). Next: `deepfield ci-local` (adds the smoke), then the runner.
- 2026-09-26 · cloud session with the owner at the GPU box · **`deepfield ci-local`: OK in 45 s**, the first end-to-end run of the pre-merge set on Windows and exactly what the `unreal-win` nightly will run: layering, ownership, schemas, coverage and plan-status ok; build 1 s (incremental); gate 20 s; smoke 22 s, with host seat 1 and client seat 2 on `L_Dev_Empty`, port 7788. The client logged `Welcomed by server` and was admitted through the dev-join branch (the expected `join without handshake options` warning on the host). Next for this workstream: register the runner (CONTRACTS/ci.md runner steps 1–8).
- 2026-09-26 · cloud session with the owner at the GPU box · **Correction to the first entry today: the Editor build is `/fp:precise`.** Every compile response file with an `/fp:` flag reads `/fp:precise` (24 of them, none `/fp:fast`), so CONTRACTS/ci.md's reading of V7 holds for Editor targets, and the "in doubt" notes are withdrawn from ci.md, windows-bringup §4, NEXT.md and runbook §8. The `+ 0.0` signed-zero fold in `DFLaneGraphBuilder.cpp` still did not hold on MSVC 14.44: the lookup miss can only come from equal keys with different bytes, the only change in `e859ffb` is how ±0 fold, and it fixed the crash. No unity-build name collision, and no documented MSVC bug found; the cause is not established. What stands is the practice: test the bits for IEEE edge cases. Game targets still need their flags read after the first package.
- 2026-09-26 · cloud session with the owner at the GPU box · **Inventory committed, engine re-pinned to 5.8.3.** The elevated inventory (`fae3e7d`) found UE **5.8.3** (CL 58210709), not 5.8.2 as today's earlier entries and commits say: every verdict today was on 5.8.3. The launcher installs only the newest hotfix, so on the owner's call the pin moves to 5.8.3: `$EnginePatch = 3` in `deepfield.ps1`, which `machine-inventory.ps1` now reads instead of keeping its own copy; the runbook's "Why a pinned patch" note moves next to the setup table in §1.2; PROGRAMME's platform row. Readiness rows fixed on the way (`b2645b4`, `d2fdfce`): .NET 8 and the GitHub CLI are optional (WARN), the runner label matches ci.md, the last column points at the current runbook sections, and launcher records without an engine no longer print errors. Defender exclusions are set (runbook §1.4 now names the clone and the DDC, not the user profile). Still missing: **Editor symbols for debugging** (the first crash's engine frames were `UnknownFunction`; the installer message had called them optional, and now says to tick them). The validator's JSON writer ended lines with CRLF on Windows, rewriting every line of the committed Foundry report: `DFMapValidate` now writes LF everywhere, and `unreal/.gitattributes` keeps a CRLF report from being committed (`git status` still flags a CRLF rewrite by size, so the writer fix is the real one). C: has 429.5 GB free of 923 GB, not the 181 GB recorded on 2026-09-24.
