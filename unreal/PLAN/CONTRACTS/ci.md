# CI — lanes, the self-hosted Mac runner, and what runs where

**Canonical:** `.github/workflows/unreal-checks.yml`, `.github/workflows/unreal-mac.yml`, `unreal/Build/*` (every workflow step is a script a developer runs by hand; nothing is CI-only). **Owner:** WS-15. **Rule:** A (a new check or lane is a PR; changing what an existing lane blocks on is INT's call). The Godot lane `.github/workflows/ci.yml` is untouched and retires with `game/` at G3.

## Lanes

| Lane | Runner | Trigger | Runs | Blocks |
|---|---|---|---|---|
| `unreal-checks` | GitHub-hosted `ubuntu-latest` | every PR and every push to `unreal/main` touching `unreal/**`, `tools/**`, `sim/**`, the unreal workflows | `layering-check.py` · `ownership-check.py --ws <from the PR title> --base origin/<base>` · `validate-content-json.py` · `plan-status.py --check` (warning only) · `content-export --diff` (dotnet 8) | merge (except the STATUS.md warning) |
| `unreal-mac` PR | self-hosted Mac `[self-hosted, macOS, ARM64, deepfield]` | PRs targeting `unreal/main` from this repository | the python checks (INT view) + `Build.sh DeepFieldEditor Mac Development -WaitMutex` | merge |
| `unreal-mac` nightly | same Mac | `cron 0 8 * * *` (03:00 America/Chicago in summer, 02:00 in winter — GitHub cron is UTC) and `workflow_dispatch` (input `ref`, default `unreal/main`) | `unreal/Build/ci-local.sh` on `unreal/main`: the INT pre-merge set (PROGRAMME.md §6.8) — checks, build, `DF.Unit+DF.Content`, listen-host smoke; a stale `STATUS.md` is a warning | the digest's "red tests" line |
| GPU box | — | — | Gauntlet, visual, perf, Windows packaging (PROGRAMME.md §7) | not yet: no box (ADR-0016) |

A PR's own test filter runs on the author's Mac (`unreal/Build/pr-check.sh`), not on the PR lane: the machine is shared with the agent sessions and a full test run per push would starve them. The nightly runs every `DF.Unit` and `DF.Content` test; `DF.Net`/`DF.Func` join it as workstreams land them (INT widens `--filter` in `ci-local.sh`).

A `STATUS.md` that no longer matches the workstream frontmatter is a **warning** on the nightly (`ci-local.sh` prints the diff and `plan-status  WARN` in its table, and still exits 0): claims and lease renewals are pushed straight to `unreal/main` between INT cycles by design (PROGRAMME.md §6.2), and `int-merge.sh` regenerates the ledger at every landing, so a red nightly for that drift would hide the red that matters. INT makes it blocking by setting the repository variable `CI_LOCAL_ARGS` to `--strict` (any other `ci-local.sh` option works there too, e.g. `--skip smoke`). `UE_ROOT`, `UE_LOCAL_DDC`, `EDITOR_LOCK_DIR` are repository variables too, with the ADR-0021 paths as defaults.

## The dev Mac as the self-hosted runner

The Mac M1 8 GB is the only machine that has the engine (ADR-0016), so it is the runner. It is registered **once, as a service, and is used by the nightly and by same-repository PRs only** — it is a development machine first, and the agent sessions share it.

0. **Where the workflow files must live.** GitHub fires `schedule` only for workflow files on the repository's *default* branch — `main`, the Godot game — and lists a workflow under *Actions → Run workflow* only if it exists there; `unreal/main` is not the default until G3 merges it. So INT lands `.github/workflows/unreal-mac.yml` and `unreal-checks.yml` on `main` as well (a `[WS-15]` PR to `main` carrying only those two files, byte-identical to `unreal/main`'s; repeat when they change). The nightly then checks out `unreal/main` explicitly (the `ref:` on its checkout step), and a dispatch takes `ref` as an input (default `unreal/main`) whatever branch the *Run workflow* dropdown shows. The `pull_request` and `push` triggers read the workflow file from the branch itself, so those lanes work from `unreal/main` alone.
1. **Register** (GitHub → repository *Settings → Actions → Runners → New self-hosted runner → macOS / ARM64*; the page shows a one-time token). Install on the external SSD so the workspace, DDC and Intermediate never touch the 16 GB internal disk:
   ```bash
   mkdir -p /Volumes/Toshiba/actions-runner && cd /Volumes/Toshiba/actions-runner
   curl -o actions-runner-osx-arm64.tar.gz -L https://github.com/actions/runner/releases/latest/download/actions-runner-osx-arm64-<version>.tar.gz
   tar xzf actions-runner-osx-arm64.tar.gz
   ./config.sh --url https://github.com/<owner>/deepfield-3d --token <token> \
       --name deepfield-mac --labels deepfield --work _work --unattended
   ./svc.sh install && ./svc.sh start        # launchd service: survives logout and reboots
   ```
   `macOS` and `ARM64` are added by the runner automatically; **`deepfield`** is the label the workflow keys on, so no other repository's workflow can land on this machine by accident. Use the repository runner registration, not an organisation one.
2. **Environment.** The workflow reads `UE_ROOT` (default `/Users/Shared/Epic Games/UE_5.8`), `DEVELOPER_DIR` (`/Applications/Xcode.app/Contents/Developer`), `UE_LOCAL_DDC` (`/Volumes/Toshiba/Deepfield-Unreal/DDC`) and `EDITOR_LOCK_DIR` (`/Volumes/Toshiba/Deepfield-Unreal/.editor-lock`). Override any of them as a repository *variable* of the same name; nothing needs to be set in the runner's `.env`. The runner user must be able to read the engine, run Xcode's toolchain (`sudo xcodebuild -license accept` once) and write to the SSD.
3. **LFS credentials.** `actions/checkout@v4` with `lfs: true` fetches LFS objects with the job's `GITHUB_TOKEN` over HTTPS — no stored credential on the machine. The runner needs `git-lfs` on its `PATH` (`brew install git-lfs`; the launchd service inherits `/opt/homebrew/bin` only if `./svc.sh` was installed from a shell that had it — check with a `workflow_dispatch` run, the first step prints `git lfs ls-files`). Push never happens from CI, so no write credential exists on the runner.
4. **Never PRs from forks.** The job carries `if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name == github.repository`, and the repository setting *Actions → General → Fork pull request workflows from outside collaborators* must stay at "Require approval for all outside collaborators". A fork PR could edit `unreal/Build/*.sh`, which the job executes as the runner user on the dev machine.
5. **Sharing the machine.** The runner is a peer of the agent sessions: builds wait on UBT's mutex (`-WaitMutex`), editor processes wait on `editor-lock.sh` (up to 30 min), and the workflow's `concurrency` group keeps CI itself to one job at a time. A nightly takes ~15–25 min when the machine is idle; queued behind a batch it may wait the full lock timeout and fail with exit 75 (editor-lock timed out) — rerun it. Cooks and packaging (later) take both editor slots (PROGRAMME.md §6.7) and will be nightly-only for that reason.
6. **Workspace hygiene.** The runner keeps one checkout under `/Volumes/Toshiba/actions-runner/_work/deepfield-3d/deepfield-3d` with its own `Intermediate/` and `Binaries/` (so a CI build never dirties the SSD working copy or an agent worktree); the DDC is shared. Logs and the automation JSON report are uploaded as the `unreal-mac-logs-<run>` artifact (14 days).
7. **Stopping it.** `./svc.sh stop` pauses the runner (queued jobs wait); `./svc.sh uninstall` then `./config.sh remove --token <token>` deregisters it.

## Test naming and where tests live

`DF.<Layer>.<Area>.<Name>` — `DF.Unit.*` (logic, `-nullrhi`), `DF.Content.*` (round-trip, bindings, tag coverage), `DF.Asset.*`, `DF.Func.*`, `DF.Net.*`, `DF.Vfx.*`, `DF.Audio.*`, `DF.Map.*`, `DF.Perf.*` — see PROGRAMME.md §7. Tests are `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (or the complex/latent forms) in the owning module's `Private/Tests/`, under `#if WITH_AUTOMATION_TESTS`; flags `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter` for logic that runs anywhere, `EAutomationTestFlags::EditorContext | ...ProductFilter` for editor-only ones. The shared helpers (`FDFTestWorld`, `FDFMessageCapture`, `Tick`) are `Source/DFCore/Public/Testing/DFTestUtils.h` (contract-append, WS-15). `DFTests` holds the cross-module and harness tests; `DF.Dev.*` names are reserved for deliberately failing/diagnostic tests and never ship.

`unreal/Build/test.sh <filter>` runs `UnrealEditor-Cmd -nullrhi -ExecCmds="Automation RunTests <filter>; Quit" -ReportExportPath=...` and takes its verdict from the controller's JSON report: any test not `Success`, or a filter that matched nothing, is exit 1; an editor that never produced a report is exit 2. Filters join with `+`.

## GPU-box lane: checks added by INT (2026-09-19)
- **Floating-point pinning on Game targets.** `BuildSettingsVersion.V7` builds Editor targets FP-precise but leaves Game/Client/Server at Default (`/fp:fast` on MSVC). Any code that must be bit-identical across hosts (`DFEnemies/Waves`: `FDFWavePlan`, `DFDetMath`, `DFDetRng`) pins precision by pragma; the Windows lane runs `DF.Unit.WavePlan*` on a **Game** target build, not only the Editor target, and fails the lane if the golden vectors drift. Also confirm `-ffp-contract` (clang) / `/fp:contract` (MSVC) do not fuse multiply-adds in those files.

## Which lanes run on an Unreal pull request (INT, 2026-09-21)
| Workflow | Runs when | Covers |
|---|---|---|
| `unreal-checks` | `unreal/**`, `tools/**`, `sim/**`, `unreal-*.yml` | python ledger checks + `content-export --diff` (hosted) |
| `unreal-mac` | nightly 08:00 UTC + `workflow_dispatch` | full Mac suite (**self-hosted; no runner is registered yet, so this lane has never run**) |
| `ci` (Godot + sim) | `game/**`, `sim/**`, `docs/**`, the art-contract scripts, its own file | the frozen client and sim; **path-filtered on 2026-09-21** |

An Unreal-only PR therefore runs `unreal-checks` and nothing else, and its verification comes from
`int-merge` on the Mac (build + `DF.Unit`+`DF.Content` + listen smoke) rather than from GitHub. That is
the honest position until a runner exists: **GitHub currently proves almost nothing about the Unreal
tree**, and no one should read a green PR as more than "the ledger and the schemas are consistent".

Why `ci` was filtered rather than fixed: its `smoke` lane aborts at Mono teardown (exit 134, "Leaked
unsafe reference") often enough that identical content failed and passed within the hour on
2026-09-21, and it gates code that is frozen and deleted at G3. Repairing a retired client's teardown
is not worth a session; the lanes still run in full whenever `game/`, `sim/` or the art contract moves,
which is the only time they can tell anyone anything.

## A red run on the Mac is unproven, not failed (INT, 2026-09-21)
The Mac has produced a test failure caused by memory exhaustion rather than by the code: the editor
asserted in `pthread_rwlock_init` (error 16) during world cleanup with 0.1 GB free and 5.9 GB of 7 GB
swap in use, then hung until the timeout, on code that had passed 37/37 twelve minutes earlier. So:
**re-run once before treating a red result from this machine as a defect**, and when you report it, say
that you re-ran — a retry that is not disclosed is indistinguishable from hiding a failure. A result
that reproduces is a result; a result that does not is a note about the machine. This is not licence to
retry until green: two reds in a row are a defect until proven otherwise, and the rule dies the day a
build machine exists that is not also the developer's laptop.

## Every test names the wrong implementation it catches (INT, 2026-09-21)
**Six times in one night, across four workstreams, a test passed for a reason its author did not
intend.** A destroy-mid-release test that survived on a copied entry and an emptied array failing the
loop condition. A siege test that pinned the branch being taken but not the price, so two factors could
be deleted from the formula. A `Begin` test pinned only in the negative, so the suite stays green if
the spawn node stops being a routing decision. A chilled-vs-unchilled assertion whose two sides land on
the same answer either way. A destroyed-director test whose fixture ended before the broadcast under
test. And poison's bypass, where the obvious fixture (real poison sets *both* flags) passes under a
swapped-assignment bug — closed only by two asymmetric rows, one that ignores armour alone and one that
ignores shields alone.

None was caught by running the tests. Every one was caught by asking: **what wrong implementation would
this still pass?**

So, the rule for every PR, not only for fix rounds: for each test you add, be able to name the line you
would revert to make it go red, and say it in the PR. WS-02 set the mechanical bar and it costs one
build — **revert your own fix in the working tree, rebuild, run, and record what went red and what did
not move.** The second half is the load-bearing half: it proves the new tests catch *these* defects
rather than catching collateral. A test whose falsifying change you cannot name is a test you have not
finished writing.

Corollary for a "safety" test: **a test for "X is safe when Y happens" must assert that Y happened.**
Two of the six were fixtures that never reached the event they existed to test — one failed for an
unrelated reason and one would have passed while testing nothing.

## A green run on a stale binary (INT, 2026-09-21)
`test.sh` now **refuses to run** when any file under `Source/`, `Config/` or the `.uproject` is newer
than the project's built modules, and says which file. `DF_TEST_ALLOW_STALE=1` overrides it and warns
loudly, because a silent override is the same bug.

Why it is in `test.sh` rather than in `pr-check.sh` where it was found: `test.sh` is the single choke
point — `pr-check`, `int-merge` and `ci-local` all route through it — so every test run is protected
rather than one flag.

What happened. WS-05 ran `pr-check --no-build` after a fix whose **build had failed** — a stray `*/`
that UHT rejected in 7 seconds. `--no-build` ran the *previous* binary and reported `tests: OK — 34
passed`, `pr-check: OK`. The author reported "all four taken" on the strength of a fix that had been
written and never compiled, and only caught it because they went back and looked at the build log.

`--no-build` is not the problem and should stay: it is the difference between a 350 s run and a 4000 s
one on this machine. The problem is that it could not tell *"the editor is built for this tree"* from
*"the editor is built for the tree as it was twenty minutes ago"*, and both produce the identical word
`OK`.

**Note what this failure defeats.** It passes the falsification rule above — revert your fix, rebuild
(fails), test the stale binary, watch the named tests go red for the reason you expected. It passes
ownership, layering and schema checks. It survives review, because the code being reviewed is correct;
it simply was not the code that ran. A verdict is only worth what the thing it ran against is worth,
and every other check in this programme assumes that link without testing it. That is the general
lesson: **when a check reports on an artefact, something must prove the artefact came from the input.**

## What a landing actually runs, and what it has never run (INT, 2026-09-21)
Counting registrations on `unreal/main` by their `IMPLEMENT_*_TEST` macro rather than by what anyone
assumed:

| suite | tests | gated a landing? |
|---|---|---|
| `DF.Unit` | 69 | yes |
| `DF.Content` | 3 | yes |
| `DF.Online` | 6 | **no** |
| `DF.Editor` | 4 | **no** |
| `DF.UI` | 4 (+39 arriving with #44) | **no** |
| `DF.Func` | 1 | **no** |

`int-merge` and `ci-local` both run `DF.Unit+DF.Content` and nothing else, so **15 of 87 registered
tests have never gated anything** — WS-11's entire online suite, WS-09's terrain and map-validate
editor tests, WS-02's `DF.Func.Status.ThermalShockInLevel` (the workstream's own Definition of Done),
and WS-12's tag-registration guard. Every one was written, run once by its author, reported green, and
then never run again.

This is the same shape as the stale binary above, one level out: **a check that reports on less than
the reader believes.** `tests: OK — 71 passed` reads as "the tests pass" and means "the two suites in
this filter pass". Nobody lied and nobody was careless; the filter was written when `DF.Unit` was all
there was, and it never grew as the suites did.

The fix is not simply to widen the filter, because a suite that has never gated may be red for reasons
nobody has looked at, and turning that into a hard gate at 5 a.m. blocks every landing. The order is:
run the full `DF` filter once to get ground truth, record what is red and why, then widen the landing
filter to everything that is green and name each exclusion with its reason. An exclusion with a reason
is a decision; a filter that silently omits four suites is an accident.

## Ground truth on the four never-gated suites (INT, 2026-09-24)
Measured, not assumed — one full run against `origin/unreal/main` at `da752b1`, build verified first:

| suite | tests | result |
|---|---|---|
| `DF.Unit` | 79 | all pass |
| `DF.Online` | 6 | **all pass** — had never gated a landing |
| `DF.Editor` | 4 | **all pass** — had never gated a landing |
| `DF.UI` | 4 | **all pass** — had never gated a landing |
| `DF.Content` | 3 | all pass |
| `DF.Func` | 1 | **passes** — had never gated a landing |

97 of ours, all green, so **all four are now in the gate** and nothing had to be excluded. The only
failure in the run was `Slate.Window.GetCurrentWindowZone.WindowedFullscreen`, an engine test about
window zones that cannot mean anything under `-nullrhi` with no real window.

**Two traps found doing this, both worth more than the result.**

1. **The automation filter is a case-insensitive SUBSTRING match, not a prefix.** `Automation RunTests
   DF` ran **230** tests, not 97: `DF` matches `Share`**`dF`**`ragments`, `Range`**`dF`**`or`,
   `An`**`dF`**`requency`, `Payloa`**`dF`**`or`. 133 engine tests came along, which is why the run took
   far longer than it should and why a naive `DF` filter is not the gate. The roots are spelled out in
   `DF_GATE_FILTER` for that reason.
2. **A filter that omits a suite is indistinguishable from a suite that does not exist**, which is why
   this went unnoticed for weeks. So the gate is now defined **once**, as `DF_GATE_FILTER` in
   `test.sh` — `int-merge`, `ci-local` and `pr-check` pass no filter and inherit it — and
   `check-test-coverage.py` fails a landing when a registered `DF.*` root is outside the gate without a
   recorded reason. Its `EXCLUDED` table holds `DF.Perf` and `DF.Soak` with the reason each cannot run
   in a landing. **An exclusion with a reason is a decision; an omission is an accident**, and the
   check is what keeps them distinguishable. Verified by falsification: with the gate set back to
   `DF.Unit+DF.Content` the checker exits 1 and names all four suites.

## Asserting log content: `AddExpectedMessage` has three traps
Established from the 5.8 source while closing WS-05's F4 gap. `FAutomationTestFramework::InternalStopTest`
(`AutomationTest.cpp:1376`) does verify expectations, and `Occurrences` defaults to 1, so a registered
message that never appears **does** fail the test. But:
- the default `CompareType` is **`Contains`** (`AutomationTest.h:161`), so a pattern that is a substring
  of two different diagnostics is satisfied by either — the assertion then proves *a* message appeared,
  not *that* one. Use `Exact` with the full sentence, or a phrase unique to the one branch.
- verbosity is a **minimum, inclusive upward**: registering at `Warning` also intercepts `Error` and
  `Fatal`, and never sees a `Display` or `Log` line.
- **there is no "must not occur".** `Occurrences = 0` means *at least once* (`:1840-1848` fails when the
  actual count is 0), so the natural way to write "the wrong diagnosis is gone" asserts the opposite of
  what it reads like. Asserting absence needs your own log sink.

## Every change to `unreal/main` goes through `int-merge` (INT, 2026-09-25)
**The GitHub merge button bypasses the only step that compiles anything.** Three incidents through that
one door, each worse than the last:

| PR | what the route cost |
|---|---|
| #40 | merged via the UI, so it skipped build, tests and smoke entirely. Known-compiling only because every later landing happened to include it. |
| #41 | stacked on #40's branch and merged into it **one minute after** #40 had already merged that branch to main, so ~1400 lines never reached the trunk and sat lost for a day (recovered as #44). |
| #48 | said **in its own description** that none of its C++ was compiled and asked for a Mac build before landing. Merged via the UI. `unreal/main` did not build for the next hour. |
| #51 | merged via the UI, also labelled unbuilt, onto the still-broken trunk. |

#48's Python checks were all green — layering, ownership, schemas, `check-test-coverage` — because none
of them need a compiler. That is not a careless author; it is the same shape as every other verification
failure in this project, one level further out: **the check ran and told you nothing.**

So the rule: **land with `unreal/Build/int-merge.sh <branch> --ws NN`, never with the merge button.**
It refuses at the build step, which is exactly where #48 needed refusing — 765 s in, with nothing on the
trunk. A PR that cannot be landed from a machine with the engine installed is not ready to land.

**Recommended to the human, sequenced, because the order matters:** register the self-hosted runner
(`windows-bringup.md` §8), *then* require that check on `unreal/main` via branch protection. Requiring a
check that has no runner blocks everything; there are currently **zero runners registered**, so today
the rule above is the only thing standing between an unbuilt PR and the trunk. Enabling protection is a
change to how every session and every human works, so it is the repository owner's call, not INT's.

