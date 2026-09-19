# CI — lanes, the self-hosted Mac runner, and what runs where

**Canonical:** `.github/workflows/unreal-checks.yml`, `.github/workflows/unreal-mac.yml`, `unreal/Build/*` (every workflow step is a script a developer runs by hand; nothing is CI-only). **Owner:** WS-15. **Rule:** A (a new check or lane is a PR; changing what an existing lane blocks on is INT's call). The Godot lane `.github/workflows/ci.yml` is untouched and retires with `game/` at G3.

## Lanes

| Lane | Runner | Trigger | Runs | Blocks |
|---|---|---|---|---|
| `unreal-checks` | GitHub-hosted `ubuntu-latest` | every PR and every push to `unreal/main` touching `unreal/**`, `tools/**`, `sim/**`, the unreal workflows | `layering-check.py` · `ownership-check.py --ws <from the PR title> --base origin/<base>` · `validate-content-json.py` · `plan-status.py --check` (warning only) · `content-export --diff` (dotnet 8) | merge (except the STATUS.md warning) |
| `unreal-mac` PR | self-hosted Mac `[self-hosted, macOS, ARM64, deepfield]` | PRs targeting `unreal/main` from this repository | the python checks (INT view) + `Build.sh DeepFieldEditor Mac Development -WaitMutex` | merge |
| `unreal-mac` nightly | same Mac | `cron 0 8 * * *` (03:00 America/Chicago in summer, 02:00 in winter — GitHub cron is UTC) and `workflow_dispatch` | `unreal/Build/ci-local.sh`: the INT pre-merge set (PROGRAMME.md §6.8) — checks, build, `DF.Unit+DF.Content`, listen-host smoke | the digest's "red tests" line |
| GPU box | — | — | Gauntlet, visual, perf, Windows packaging (PROGRAMME.md §7) | not yet: no box (ADR-0016) |

A PR's own test filter runs on the author's Mac (`unreal/Build/pr-check.sh`), not on the PR lane: the machine is shared with the agent sessions and a full test run per push would starve them. The nightly runs every `DF.Unit` and `DF.Content` test; `DF.Net`/`DF.Func` join it as workstreams land them (INT widens `--filter` in `ci-local.sh`).

The nightly tolerates a stale `STATUS.md` only if INT sets the repository variable `CI_LOCAL_ARGS` to `--skip plan-status`; by default a ledger that no longer matches the workstream frontmatter is a red nightly with a one-line fix (`python3 unreal/Build/plan-status.py`, commit). `UE_ROOT`, `UE_LOCAL_DDC`, `EDITOR_LOCK_DIR` are repository variables too, with the ADR-0021 paths as defaults.

## The dev Mac as the self-hosted runner

The Mac M1 8 GB is the only machine that has the engine (ADR-0016), so it is the runner. It is registered **once, as a service, and is used by the nightly and by same-repository PRs only** — it is a development machine first, and the agent sessions share it.

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
