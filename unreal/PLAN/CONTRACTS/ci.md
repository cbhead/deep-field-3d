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

