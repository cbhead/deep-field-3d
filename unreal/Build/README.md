# unreal/Build — scripts (WS-15)

Everything CI runs is a script here, and every script runs by hand the same way. Paths are relative to the repository root; `python3` is the system 3.9+ (no third-party packages; otherwise [python.org](https://www.python.org/downloads/)), the shell scripts are zsh. The engine ([Unreal Engine 5.8.2](https://www.unrealengine.com/download) through the [Epic Games Launcher](https://store.epicgames.com/download)) is found through `UE_ROOT` (default `/Users/Shared/Epic Games/UE_5.8`); [Xcode](https://apps.apple.com/app/xcode/id497799835) through `DEVELOPER_DIR` (default `/Applications/Xcode.app/Contents/Developer`). The full install list with versions is in [../README.md](../README.md#12-software).

## Before a PR / for INT

| Script | What it does | Exit |
|---|---|---|
| `pr-check.sh [--ws NN] [--filter F] [--no-build] [--no-tests] [--smoke]` | The workstream pre-PR set (PROGRAMME.md §6.5): layering → ownership for **your** workstream (from the branch name `ws/NN-…` or `--ws`) → content schemas → editor build → `F+DF.Content` tests under the editor lock (default `DF.Unit+DF.Content`). | 1 at the first failure |
| `ci-local.sh [--skip STEP]… [--filter F] [--client-count N] [--ws ID --base REF] [--strict]` | The INT pre-merge set (§6.8) and the nightly: layering, ownership (INT view), schemas, `check-test-coverage.py`, `plan-status.py --check`, `Build.sh -WaitMutex`, `editor-lock.sh test.sh F`, `editor-lock.sh smoke-listen.sh -client-count N`. Prints a summary table on exit. Steps: `layering ownership schema coverage plan-status build tests smoke`. A stale `STATUS.md` is a **warning** (`WARN` in the table, the diff printed) unless `--strict`: claims and lease renewals land on `unreal/main` between INT cycles by design and `int-merge.sh` regenerates the ledger at every landing. ~15 min idle; 30+ when other agents build. | 1 at the first failure |
| `int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run]` (INT) | Lands one workstream branch on `unreal/main`: rebases it in a temp worktree, runs the checks, regenerates `STATUS.md` (amended into the last commit if stale), builds, runs `DF.Unit+DF.Content`, the smoke, then fast-forwards and pushes. | 1 at the first failure; the temp worktree is left for inspection |

## Checks (python, seconds, no engine)

| Script | What it does | Exit |
|---|---|---|
| `layering-check.py` | Every `DF*.Build.cs` may depend only on DF modules in a **lower** layer of `modules.json`. | 1 on a violation |
| `ownership-check.py --ws NN\|INT [--base REF] [--files …]` | The diff `REF...HEAD` (default `origin/unreal/main`) against the glob table in `PLAN/OWNERSHIP.md`: a **binary** outside your globs is a violation, a text file outside is a warning (PR to the owner or RFC). `--ws INT` lists everything and fails nothing. | 1 on a violation |
| `check-test-coverage.py [--list]` | Every registered `DF.*` suite is in the landing gate (`DF_GATE_FILTER` in `test.sh`, the one definition `int-merge.sh`, `ci-local.sh` and `pr-check.sh` inherit) or in its `EXCLUDED` table with the reason it cannot run in a landing (CONTRACTS/ci.md, 2026-09-24). `--list` prints each suite and its verdict. | 1 on a registered suite outside the gate |
| `validate-content-json.py [table…]` | `unreal/content/json/*.json` against `unreal/content/schema/*.schema.json` (small in-house JSON-Schema subset), plus envelope `table` and duplicate ids. | 1 on any failure |
| `plan-status.py [--out F] [--check]` | Regenerates `PLAN/STATUS.md` from the workstream frontmatter. `--check` regenerates in memory at the committed file's own "Generated" instant and diffs: stale = exit 1 with "run plan-status.py". `--out` writes elsewhere (a temp file). | `--check`: 1 if stale |
| `plan-claim.py ws-NN --owner H --branch B` (and its renew/release/update forms, see `--help`) | Claims, renews, releases or updates a workstream: edits only that workstream file (owner, `claimed_at`, `lease_expires` +24 h, branch, state). The one direct push to `unreal/main` (PLAN/README.md). | |
| `plan-scaffold.py` | Creates any workstream file missing from `PLAN/registry.json` (never overwrites). | |
| `new-module.py [Module]` | Generates the missing module skeletons (Build.cs + module .h/.cpp) from `modules.json`; never overwrites (INT: a new module also touches the `.uproject` and targets). | |
| `plan_lib.py` | Shared frontmatter parsing for the `plan-*` scripts (not a command). | |
| `modules.json` | The module layering table (INT-owned). Read by `layering-check.py` and `new-module.py`. | |

## Engine (Mac; one editor process machine-wide)

| Script | What it does | Exit |
|---|---|---|
| `editor-lock.sh <command…>` | Runs a command holding the machine-wide editor lock (`EDITOR_LOCK_DIR`, default `/Volumes/Toshiba/Deepfield-Unreal/.editor-lock`); waits up to `EDITOR_LOCK_TIMEOUT` s (1800) for its turn, reclaims a lock whose pid is gone once it is 60 s old, runs the command as a child (never `exec` — zsh skips the EXIT trap on exec and the lock leaked), forwards INT/TERM to it, releases on exit, and exports the DDC path (`UE_LOCAL_DDC`). **Every** `UnrealEditor-Cmd` run on the shared Mac goes through it: `editor-lock.sh unreal/Build/test.sh DF.Unit`, `editor-lock.sh unreal/Build/smoke-listen.sh`. | the command's; 75 on timeout |
| `test.sh [filter]` | `UnrealEditor-Cmd -nullrhi -unattended -nop4 -nosplash -NoSound -ExecCmds="Automation RunTests <filter>; Quit" -ReportExportPath=…`. Default filter `DF.Unit+DF.Content+DF.Net`; `+` joins filters. The verdict is the controller's JSON report (`Saved/Automation/Reports/test-<filter>/index.json`), never the editor's exit code: prints `PASS`/`FAIL` per test with the failing test's errors. A filter that matches **no** test fails (`DF_TEST_ALLOW_EMPTY=1` to allow). `DF_TEST_TIMEOUT` s (1800) kills a hung editor. Log: `Saved/Logs/test-<filter>.log`. | 0 all passed · 1 a failure or nothing matched · 2 no result |
| `smoke-listen.sh [map] [-client-count N] [-map PATH] [-port P]` | A listen host on `/Game/DF/Dev/L_Dev_Empty` (or `map`) and N headless clients (default 1) that join it; OK only if the host logged host+N `player joined` and every client reached the map. `DF_SMOKE_TIMEOUT` s (180) to wait for the joins. Logs: `Saved/Logs/smoke-host.log`, `smoke-client-<n>.log`. Hold the lock for the whole smoke. | 0 / 1 / 2 (bad args or no editor) |

The build itself is the engine's script, always with `-WaitMutex` so concurrent builds queue instead of thrashing 8 GB:
```
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development -Project="$PWD/unreal/DeepField/DeepField.uproject" -WaitMutex -NoHotReload | grep -E " error |Result:"
```

## Windows (the GPU workstation)

Nothing above runs on Windows yet: the shell scripts are zsh and Mac-pathed. [windows-bringup.md](windows-bringup.md) is the checklist that takes a bare Windows machine to a building, testing, packaging runner with the raw engine commands; the `.ps1`/`.bat` twins of these scripts and `unreal-win.yml` are WS-15 work that starts once it is green.

The exception is `machine-inventory.ps1`, which runs on Windows and is read-only. It measures the box (hardware, driver, DirectX, disks, toolchain, engine installs, git config, runner) and writes [machines/windows-gpu.md](machines/windows-gpu.md) plus a `.json`. That file includes a readiness table against `windows-bringup.md`. Read it instead of assuming what the box has, and re-run and commit it after any install. See [machines/README.md](machines/README.md) for the Mac and cloud counterparts.

## Where the results go

`unreal/DeepField/Saved/` is untracked: `Logs/test-*.log`, `Logs/smoke-*.log`, `Logs/ci-build.log`, `Logs/pr-build.log`, `Automation/Reports/test-*/index.json` (+ an `index.html` you can open). CI uploads the same files as the `unreal-mac-logs-<run>` artifact. The lanes and the self-hosted runner are described in `unreal/PLAN/CONTRACTS/ci.md`.
