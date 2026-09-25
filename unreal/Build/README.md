# unreal/Build — scripts (WS-15)

Everything CI runs is a script here, and every script runs by hand the same way. Paths are relative to
the repository root. There are three kinds:

- **Windows scripts** (`*.ps1`): the everyday path on the only engine machine (ADR-0028).
- **Python checks** (`*.py`) run anywhere with Python 3.9+ and no third-party packages: on Windows
  (`python`, or through `deepfield check`), in the hosted CI lane, and in any checkout (`python3`).
- **zsh scripts** (`*.sh`) were written for the retired Mac. They hard-code Mac engine paths
  (`Engine/Binaries/Mac`, `BatchFiles/Mac/Build.sh`, `.dylib` timestamps), so they **do not run on
  Windows, or anywhere else now**. They stay as the specification for their Windows ports, which WS-15
  builds onto `deepfield.ps1` rather than as a second set of wrappers.

## Windows

| Script | What it does | Exit |
|---|---|---|
| `deepfield.ps1` (start it with `unreal\deepfield.cmd <command>`) | The whole Windows path in one script ([runbook](../README.md) §1). `setup` checks and installs every prerequisite (winget: Git, Git LFS, Python, Visual Studio 2022 with an MSVC toolset the installed engine's `Windows_SDK.json` accepts; the Epic launcher for the engine), clones with LFS and `core.autocrlf=false`, sets `UE_ROOT`, builds. `doctor` checks without changing anything. `build`; `test [filter]` (reads `DF_GATE_FILTER` from `test.sh`, keeps its stale-binary refusal and JSON-report verdict); `pr-check [filter] [-Ws NN] [-Base REF]` (`pr-check.sh`'s job: the checks plus your workstream's ownership check, the build, then the landing gate; a filter makes it `PARTIAL`); `check` (layering, schemas, test coverage); `editor`; `play`/`host`/`join`; `solution`. | 1 when a check or step fails; 2 on a script error |
| `machine-inventory.ps1 [-NoDxDiag] [-Name N]` | Read-only. Measures the box (hardware, driver, DirectX, disks, toolchain, engine installs, git config, runner) and writes [machines/windows-gpu.md](machines/windows-gpu.md) plus a `.json`, including a readiness table against [windows-bringup.md](windows-bringup.md). Read it instead of assuming what the box has; re-run and commit it after any install ([machines/README.md](machines/README.md)). Run it from PowerShell or cmd, not Git Bash. | |

## Checks (python, seconds, no engine)

| Script | What it does | Exit |
|---|---|---|
| `layering-check.py` | Every `DF*.Build.cs` may depend only on DF modules in a **lower** layer of `modules.json`. | 1 on a violation |
| `ownership-check.py --ws NN\|INT [--base REF] [--files …]` | The diff `REF...HEAD` (default `origin/unreal/main`) against the glob table in `PLAN/OWNERSHIP.md`: a **binary** outside your globs is a violation, a text file outside is a warning (PR to the owner or RFC). `--ws INT` lists everything and fails nothing. | 1 on a violation |
| `check-test-coverage.py [--list]` | Every registered `DF.*` suite is in the landing gate (`DF_GATE_FILTER` in `test.sh`, the one definition every landing inherits) or in its `EXCLUDED` table with the reason it cannot run in a landing (CONTRACTS/ci.md, 2026-09-24). `--list` prints each suite and its verdict. | 1 on a registered suite outside the gate |
| `validate-content-json.py [table…]` | `unreal/content/json/*.json` against `unreal/content/schema/*.schema.json` (small in-house JSON-Schema subset), plus envelope `table` and duplicate ids. | 1 on any failure |
| `plan-status.py [--out F] [--check]` | Regenerates `PLAN/STATUS.md` from the workstream frontmatter. `--check` regenerates in memory at the committed file's own "Generated" instant and diffs: stale = exit 1 with "run plan-status.py". `--out` writes elsewhere (a temp file). | `--check`: 1 if stale |
| `plan-claim.py ws-NN --owner H --branch B` (and its renew/release/update forms, see `--help`) | Claims, renews, releases or updates a workstream: edits only that workstream file (owner, `claimed_at`, `lease_expires` +24 h, branch, state). Does not commit; the commit and the direct push to `unreal/main` are in PLAN/README.md, "Claiming in one command". | |
| `plan-scaffold.py` | Creates any workstream file missing from `PLAN/registry.json` (never overwrites). | |
| `new-module.py [Module]` | Generates the missing module skeletons (Build.cs + module .h/.cpp) from `modules.json`; never overwrites (INT: a new module also touches the `.uproject` and targets). | |
| `plan_lib.py` | Shared frontmatter parsing for the `plan-*` scripts (not a command). | |
| `modules.json` | The module layering table (INT-owned). Read by `layering-check.py` and `new-module.py`. | |

## Engine scripts (zsh, Mac-era; awaiting their Windows ports)

What each one does is the specification its port must meet. The last column is how to do the same job
on Windows today.

| Script | What it does | On Windows today |
|---|---|---|
| `test.sh [filter]` | `UnrealEditor-Cmd -nullrhi -unattended -nop4 -nosplash -NoSound -ExecCmds="Automation RunTests <filter>; Quit" -ReportExportPath=…`. Default filter: the landing gate, `DF_GATE_FILTER` = `DF.Unit+DF.Content+DF.Online+DF.Editor+DF.UI+DF.Func`; `+` joins filters. **Refuses to run** if any file under `Source/`, `Config/` or the `.uproject` is newer than the built modules (`DF_TEST_ALLOW_STALE=1` overrides, loudly). The verdict is the controller's JSON report (`Saved/Automation/Reports/test-<filter>/index.json`), never the editor's exit code: prints `PASS`/`FAIL` per test with the failing test's errors. A filter that matches **no** test fails (`DF_TEST_ALLOW_EMPTY=1` to allow). `DF_TEST_TIMEOUT` s (1800) kills a hung editor. Exit 0 all passed · 1 a failure or nothing matched · 2 no result. | `deepfield test [filter]` (runbook §5.1) |
| `smoke-listen.sh [map] [-client-count N] [-map PATH] [-port P]` | A listen host on `/Game/DF/Dev/L_Dev_Empty` (or `map`) and N headless clients (default 1) that join it; OK only if the host logged host+N `player joined` and every client reached the map. `DF_SMOKE_TIMEOUT` s (180) to wait for the joins. Logs: `Saved/Logs/smoke-host.log`, `smoke-client-<n>.log`. | runbook §5.3 |
| `pr-check.sh [--ws NN] [--filter F] [--no-build] [--no-tests] [--smoke]` | The workstream pre-PR set (PROGRAMME.md §6.5): layering → ownership for **your** workstream (from the branch name `ws/NN-…` or `--ws`) → content schemas → editor build → the landing gate (`DF_GATE_FILTER`, the same set `int-merge` and `ci-local` run). `--filter F` runs only `F`, for iterating, and ends `pr-check: PARTIAL` rather than `OK`; so does `--no-tests`. | `deepfield pr-check [filter] [-Ws NN]` (runbook §5.4) |
| `ci-local.sh [--skip STEP]… [--filter F] [--client-count N] [--ws ID --base REF] [--strict]` | The INT pre-merge set (§6.8) and the nightly: layering, ownership (INT view), schemas, `check-test-coverage.py`, `plan-status.py --check`, the editor build, the landing gate, the smoke. Prints a summary table on exit. A stale `STATUS.md` is a **warning** unless `--strict`: claims and lease renewals land on `unreal/main` between INT cycles by design, and a landing regenerates the ledger. | runbook §3, §4, §5.1–5.3 in order (`deepfield check`, `deepfield test`, the smoke by hand) |
| `int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run] [--resume]` (INT) | Lands one workstream branch on `unreal/main`: rebases it in the persistent verify worktree `int-verify` (serialised by `int-verify.lock`; builds stay incremental), runs the checks, builds, runs the landing gate and the smoke, commits a regenerated `STATUS.md` as its own commit, then pushes the branch and `unreal/main`. `--resume` continues a landing the worktree holds (e.g. after a hand-resolved rebase conflict). | by hand, in a throwaway worktree: rebase, runbook §3–5, `plan-status.py`, then push (never the GitHub merge button: CONTRACTS/ci.md, 2026-09-25) |
| `editor-lock.sh <command…>` | Ran a command holding a machine-wide editor lock, so that the 8 GB Mac never held two editors at once, and exported the DDC path. | nothing to do: the box has the memory for several editors. WS-15 decides whether anything replaces it. |

## Where the results go

`unreal/DeepField/Saved/` is untracked: `Logs/build-editor.log`, `Logs/test-*.log`, `Logs/smoke-*.log`, and
`Automation/Reports/<name>/index.json` (+ an `index.html` you can open). The lanes and the self-hosted
runner are described in `unreal/PLAN/CONTRACTS/ci.md`. `deepfield.ps1` writes its own setup transcript to `%LOCALAPPDATA%\DeepField\`. The one-time work that commissions the GPU box is [windows-bringup.md](windows-bringup.md).
