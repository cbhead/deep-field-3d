# CI — lanes, the self-hosted runner, and what runs where

**Canonical:** `.github/workflows/unreal-checks.yml`, `.github/workflows/unreal-win.yml`, `unreal/Build/*` (every workflow step is a script a developer runs by hand; nothing is CI-only). **Owner:** WS-15. **Rule:** A (a new check or lane is a PR; changing what an existing lane blocks on is INT's call). The Godot lane `.github/workflows/ci.yml` is not one of these and retires with `game/` at G3; it is path-filtered on PRs (below) and, since ADR-0029, on pushes to `main` too, so an Unreal-only landing does not run it. `.github/workflows/unreal-mac.yml`, the retired Mac's lane, never ran (no runner was ever registered) and was removed on 2026-09-25; WS-15 replaces it with a Windows lane on the GPU box (ADR-0028).

## Lanes

| Lane | Runner | Trigger | Runs | Blocks |
|---|---|---|---|---|
| `unreal-checks` | GitHub-hosted `ubuntu-latest` | every PR and every push to `main` touching `unreal/**`, `tools/**`, `sim/**`, the unreal workflows | `layering-check.py` · `ownership-check.py --ws <from the PR title> --base origin/<base>` · `validate-content-json.py` · `check-test-coverage.py` · `plan-status.py --check` (warning only) · `content-export --diff` (dotnet 8) | merge (except the STATUS.md warning) |
| `unreal-win` PR *(armed by `WIN_RUNNER_READY`)* | self-hosted `[self-hosted, Windows, X64, deepfield]` | PRs targeting `main` from this repository, touching `unreal/**`, `tools/**` or the unreal workflows | `deepfield ci-local -Skip tests,smoke`: the python checks (INT view) + the `DeepFieldEditor Win64 Development` build | merge |
| `unreal-win` nightly *(armed by `WIN_RUNNER_READY`)* | same | `cron 0 8 * * *` (03:00 America/Chicago in summer, 02:00 in winter — GitHub cron is UTC) and `workflow_dispatch` (input `ref`, default `main`) | `deepfield ci-local`, the INT pre-merge set (PROGRAMME.md §6.8), on `main`: checks, build, the landing gate (`DF_GATE_FILTER`), listen-host smoke; a stale `STATUS.md` is a warning | the digest's "red tests" line |
| GPU box render lanes *(planned)* | same, in an interactive session (below) | nightly | Gauntlet, visual (`DF.Vfx.EveryCueDraws`), perf (`DF.Perf.<Map>`), Windows packaging, the Game-target FP check (PROGRAMME.md §7) | the digest |

The `unreal-win` lanes are written (`.github/workflows/unreal-win.yml`; every step is `unreal\deepfield.cmd ci-local`) but **armed only once a runner exists**: their automatic triggers run when the repository variable `WIN_RUNNER_READY` is `true`, and show as skipped until then, rather than sitting queued on every PR for 24 hours. A manual dispatch always runs. Until the lane is armed, `unreal-checks` is the only lane that runs on an Unreal PR, and verification is a by-hand run on the box (`unreal/README.md` §5.4–§5.5). *Planned* (the render lanes) means not written yet (WS-15).

The PR lane builds but runs no tests: the box is shared with the agent sessions, and a full test run per push would starve them. A PR's own tests run on the box before the PR opens: `deepfield pr-check`, which ends with the landing gate. The nightly runs the whole landing gate, the one filter every landing inherits (`DF_GATE_FILTER` in `unreal/Build/test.sh`, enforced by `check-test-coverage.py`).

A `STATUS.md` that no longer matches the workstream frontmatter is a **warning** on the nightly, not a failure: claims and lease renewals are pushed straight to `main` between INT cycles by design (PROGRAMME.md §6.2), and every landing regenerates the ledger, so a red nightly for that drift would hide the red that matters. INT makes it blocking with `-Strict`, passed through the repository variable `CI_LOCAL_ARGS` (any other `deepfield ci-local` option works there too, e.g. `-Skip smoke`).

## The GPU box as the self-hosted runner

The box is registered **once, at repository level, and is used by the nightly and by same-repository PRs only**. It is a development machine too, and the agent sessions share it.

0. **Where the workflow files must live.** GitHub fires `schedule` only for workflow files on the repository's *default* branch — `main` — and lists a workflow under *Actions → Run workflow* only if it exists there. `main` is also the trunk for the Unreal work (ADR-0029; `unreal/main` is frozen), so each workflow lives there once and serves every trigger: there is no second copy of `unreal-win.yml` to keep byte-identical, and `unreal-checks.yml` sits beside it with the `unreal/` tree it checks. The nightly names `main` explicitly (the `ref:` on its checkout step), and a dispatch takes `ref` as an input (default `main`) whatever branch the *Run workflow* dropdown shows. The `pull_request` and `push` triggers read the workflow file from the branch itself.
1. **Register.** GitHub → repository *Settings → Actions → Runners → New self-hosted runner → Windows / x64* (the page shows the download command and a one-time token; the zips are also on [actions/runner releases](https://github.com/actions/runner/releases)). Unpack to a short path such as `C:\actions-runner`, then:
   ```bat
   config.cmd --url https://github.com/cbhead/deep-field-3d --token <token> ^
       --name deepfield-gpu --labels deepfield --work _work --unattended
   ```
   `self-hosted`, `Windows` and `X64` are added by the runner automatically; **`deepfield`** is the label the workflows key on, so no other repository's workflow can land on this machine by accident. Use the repository registration, not an organisation one.
2. **Service or interactive — decide per lane, and record the choice here.** A Windows service (`config.cmd --runasservice`) runs in session 0 with no desktop. That is fine for builds, cooks, packaging and `-nullrhi` tests. It is *not* a sound home for the render lanes — `DF.Vfx.EveryCueDraws`, `DF.Perf.<Map>`, Gauntlet with a real RHI — which want a logged-in desktop session on the GPU. For those, run the runner interactively (`run.cmd` from a scheduled task "at log on" of an auto-logon build account).
3. **Environment.** The repository *variable* `UE_ROOT` is optional: `deepfield.ps1` finds the engine through the launcher's machine-wide records, so set it only for an engine in an unusual place (e.g. `C:\Program Files\Epic Games\UE_5.8`). In the runner's `.env` file (`C:\actions-runner\.env`) set `UE-LocalDataCachePath` to the `DDC` folder beside the working clone (for example `D:\DF\DDC`): the runner's checkout sits elsewhere, so without it `DefaultEngine.ini`'s `%GAMEDIR%../../../DDC` would give CI a DDC of its own instead of sharing the box's.
4. **Git on the runner account.** The runner may run as a different Windows account from the developer, so the developer's `--global` git settings do not reach it. Set them system-wide from an administrator prompt: `git config --system core.longpaths true` and `git config --system core.autocrlf false` (Git for Windows' installer defaults to `autocrlf=true`, and a CRLF checkout changes the content hash, PROGRAMME.md B§5 rule 9). `actions/checkout@v4` with `lfs: true` fetches LFS objects with the job's `GITHUB_TOKEN` over HTTPS — no stored credential on the machine; `git-lfs` ships with Git for Windows. Push never happens from CI, so no write credential exists on the runner. **Git and Python must be findable by the runner's account.** `actions/checkout` needs `git` and `git-lfs` on the runner's PATH (the box's Git is installed but not on PATH today, per `Build/machines/windows-gpu.md`: add `C:\Program Files\Git\cmd` to the system PATH, or to the runner's `.path` file, and restart the runner). `deepfield.ps1` finds Python on PATH, through the `py` launcher, or in *that account's* per-user install folder, so a per-user Python works for a runner that runs as the developer's account and not for a service account: install Python for all users in that case.
5. **Never PRs from forks.** Every job carries `if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name == github.repository`, and the repository setting *Actions → General → Fork pull request workflows from outside collaborators* must stay at "Require approval for all outside collaborators". A fork PR could edit `unreal/Build/*`, which the job executes as the runner account on the dev machine.
6. **Sharing the machine.** The runner is a peer of the agent sessions: builds wait on UBT's mutex (`-WaitMutex`), and the workflow's `concurrency` group keeps CI itself to one job at a time. Cooks and packaging are nightly-only.
7. **Workspace hygiene.** The runner keeps one checkout under `C:\actions-runner\_work` with its own `Intermediate\` and `Binaries\`, so a CI build never dirties the working clone or an agent worktree; the DDC is shared (step 3). Logs, the automation JSON report and the `deepfield` transcript are uploaded as the `unreal-win-logs-<run>` artifact (14 days), and ci-local's summary table goes on the run's page. The job deletes the previous run's logs before its checkout, so an artifact never carries another run's results.
8. **Arm the lane, in this order.** (a) `unreal-win.yml` must be on `main` (already true: `main` is the trunk, ADR-0029; step 0): *Run workflow* and the nightly exist only for workflow files on the default branch. Until the variable is set, the nightly shows as skipped. (b) *Actions → unreal-win → Run workflow* with `full` ticked and `ref` = `main`, and read the summary and the artifact. (c) When that run is green, set the repository variable `WIN_RUNNER_READY` to `true` (*Settings → Secrets and variables → Actions → Variables*). The PR lane and the nightly then run on their own. (d) Only after that, consider requiring the check through branch protection (the owner's call; see the 2026-09-25 ruling below). To disarm, delete the variable.
9. **Stopping it.** Stop the service (or close `run.cmd`) to pause the runner; queued jobs wait. Delete `WIN_RUNNER_READY` first so nothing queues behind it. `config.cmd remove --token <token>` deregisters it.

## Test naming and where tests live

`DF.<Layer>.<Area>.<Name>` — `DF.Unit.*` (logic, `-nullrhi`), `DF.Content.*` (round-trip, bindings, tag coverage), `DF.Online.*`, `DF.Editor.*`, `DF.UI.*`, `DF.Asset.*`, `DF.Func.*`, `DF.Net.*`, `DF.Vfx.*`, `DF.Audio.*`, `DF.Map.*`, `DF.Perf.*` — see PROGRAMME.md §7. `check-test-coverage.py --list` shows which roots have tests today; `DF.Net` has none yet (the listen-host smoke stands in for `DF.Net.ListenHostPlusClient`). Tests are `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (or the complex/latent forms) in the owning module's `Private/Tests/`, under `#if WITH_AUTOMATION_TESTS`; flags `EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter` for logic that runs anywhere, `EAutomationTestFlags::EditorContext | ...ProductFilter` for editor-only ones. The shared helpers (`FDFTestWorld`, `FDFMessageCapture`, `Tick`) are `Source/DFCore/Public/Testing/DFTestUtils.h` (contract-append, WS-15). `DFTests` holds the cross-module and harness tests; `DF.Dev.*` names are reserved for deliberately failing/diagnostic tests and never ship.

`unreal/Build/test.sh <filter>` runs `UnrealEditor-Cmd -nullrhi -ExecCmds="Automation RunTests <filter>; Quit" -ReportExportPath=...` and takes its verdict from the controller's JSON report: any test not `Success`, or a filter that matched nothing, is exit 1; an editor that never produced a report is exit 2. Filters join with `+`.

## GPU-box lane: checks added by INT (2026-09-19)
- **Floating-point pinning on Game targets.** `BuildSettingsVersion.V7` builds Editor targets FP-precise but leaves Game/Client/Server at Default (`/fp:fast` on MSVC). Any code that must be bit-identical across hosts pins precision by pragma (the `DF_DET_FP_*` macros in `DFCore/Public/Determinism/` — `DFDetMath.h`, `DFDetRng.h`, moved there from `DFEnemies/Waves` by ruling R2 — used by `FDFWavePlan`, `FDFLaneWalker` and `DFTowerMath`); the Windows lane runs `DF.Unit.WavePlan*` on a **Game** target build, not only the Editor target, and fails the lane if the golden vectors drift. Also confirm `-ffp-contract` (clang) / `/fp:contract` (MSVC) do not fuse multiply-adds in those files.

## Which lanes run on an Unreal pull request (INT, 2026-09-21)
| Workflow | Runs when | Covers |
|---|---|---|
| `unreal-checks` | `unreal/**`, `tools/**`, `sim/**`, `unreal-*.yml` | python ledger checks + `content-export --diff` (hosted) |
| `ci` (Godot + sim) | `game/**`, `sim/**`, `docs/**`, the art-contract scripts, its own file | the frozen client and sim; **path-filtered on 2026-09-21** |

An Unreal-only PR therefore runs `unreal-checks` and nothing else, and its verification comes from
`int-merge` on the Mac (build + `DF.Unit`+`DF.Content` + listen smoke) rather than from GitHub.
*(2026-09-25, ADR-0028: the Mac is retired, so `unreal-mac` never ran (removed the same day) and `int-merge` has no
machine until its Windows port. Verification is a by-hand run on the GPU box; the current lanes are
at the top of this file.)* That is
the honest position until a runner exists: **GitHub currently proves almost nothing about the Unreal
tree**, and no one should read a green PR as more than "the ledger and the schemas are consistent".

Why `ci` was filtered rather than fixed: its `smoke` lane aborts at Mono teardown (exit 134, "Leaked
unsafe reference") often enough that identical content failed and passed within the hour on
2026-09-21, and it gates code that is frozen and deleted at G3. Repairing a retired client's teardown
is not worth a session; the lanes still run in full whenever `game/`, `sim/` or the art contract moves,
which is the only time they can tell anyone anything.

## A red run on the Mac is unproven, not failed (INT, 2026-09-21)
*(2026-09-25: measured on the 8 GB Mac, now retired (ADR-0028). The rule below stands until INT has
verdicts from the GPU box to judge it by.)*
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
*(Resolved 2026-09-24: every suite is now in the gate — see the next section.)*
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
   `test.sh` — `int-merge`, `ci-local` and `pr-check` pass no filter and inherit it (pr-check only since 2026-09-25; until then it defaulted to `DF.Unit+DF.Content` despite this line) — and
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
*(ADR-0028, the same day: `int-merge.sh` is Mac-only and now has no machine. The rule's substance
stands unchanged: a branch lands only after a build, the landing gate and the smoke have run on the GPU
box against the rebased tree — `unreal\deepfield ci-local` in a throwaway worktree holding the rebased
branch, then the push by hand, until WS-15 ports `int-merge` onto `deepfield.ps1`. The runner step below is `windows-bringup.md` §5.)*
*(ADR-0029, 2026-09-26: the Unreal trunk is now `main`, so the rule, and the branch-protection
recommendation at the end of this section, apply to Unreal changes landing on `main`. A check required
on `main` must still let §6.2's direct ledger pushes through: scope it to pull requests, or give
ledger and INT pushes a bypass.)*
**The GitHub merge button bypasses the only step that compiles anything.** Four incidents through that
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
(`windows-bringup.md` §5), *then* require that check on `unreal/main` via branch protection. Requiring a
check that has no runner blocks everything; there are currently **zero runners registered**, so today
the rule above is the only thing standing between an unbuilt PR and the trunk. Enabling protection is a
change to how every session and every human works, so it is the repository owner's call, not INT's.

