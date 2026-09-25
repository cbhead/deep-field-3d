# Deep Field 3D on Unreal Engine 5.8 — runbook

This is the Unreal rebuild of Deep Field 3D. It lives on the `unreal/main` branch, beside the frozen
Godot client (`../game/`) and the C# sim (`../sim/`, which is now the written spec). This page takes
you from a bare Mac to a built project with its tests passing, then to playing a map.

**Who this is for:** anyone setting up the Mac to build, test or play the Unreal project. The Windows
GPU workstation has its own checklist, [Build/windows-bringup.md](Build/windows-bringup.md), and
registering the CI runner is covered in [PLAN/CONTRACTS/ci.md](PLAN/CONTRACTS/ci.md).

**How to read it:** do the sections in order. Each step says what to run and what you should see. If
you see something else, stop and look it up in [Troubleshooting](#8-troubleshooting) before going on.

> Every command here comes from this repository's own scripts and config (`Build/*.sh`, `.lfsconfig`,
> `DeepField/Config/DefaultEngine.ini`). If a step turns out different on your machine, correct this
> file in the same change as whatever you had to do.

---

## 1. Prerequisites

### 1.1 Hardware

| What | Minimum | Why |
|---|---|---|
| Mac | Apple Silicon (M1 or later), **8 GB RAM** | The project's floor. If it runs here it runs anywhere (ADR-0016). More RAM makes everything faster and test verdicts more reliable ([Troubleshooting](#8-troubleshooting)). |
| External SSD | NVMe, APFS-formatted, **≥ 150 GB free** | The clone, the Derived Data Cache (DDC) and the build output do not fit on a small internal disk (ADR-0016/0021). The defaults below assume the SSD is mounted at `/Volumes/Toshiba`; if yours has another name, set the variables in [§2.4](#24-environment-variables). |
| Network | Needed for the clone | Git LFS downloads the binary assets during the clone (about 1–2 GB today). |

### 1.2 Software

Install everything in this table before starting. The last column is how to check that it is installed.

| Tool | Version | How to install | Check |
|---|---|---|---|
| macOS | 26.4 or later (the version the project is developed on) | Software Update | `sw_vers -productVersion` |
| Xcode | 26.6 (full Xcode, not only the Command Line Tools) | App Store, then run `sudo xcodebuild -license accept` once | `xcodebuild -version` |
| Epic Games Launcher | current | epicgames.com | — |
| **Unreal Engine** | **5.8.2** exactly | Launcher → Unreal Engine → Library → **+** → 5.8.2. Keep the default location `/Users/Shared/Epic Games/UE_5.8`. | `ls "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"` |
| Homebrew | current | brew.sh | `brew --version` |
| Git | 2.40+ | `brew install git` (or Xcode's) | `git --version` |
| **Git LFS** | 3.x | `brew install git-lfs`, then **`git lfs install`** once | `git lfs version` |
| Python | 3.9+ (the system `python3` is fine; no packages needed) | built in | `python3 --version` |
| zsh | the macOS default shell | built in | `zsh --version` |
| .NET SDK 8 | *optional*; only for `tools/content-export --diff` | `brew install --cask dotnet-sdk` | `dotnet --version` |

**Why exactly 5.8.2:** a different engine patch version re-saves every asset it opens. On a shared LFS
repository that shows up as a wall of binary changes nobody meant to make.

### 1.3 Accounts and access

- A GitHub account with read access to `cbhead/deep-field-3d`, plus write access if you will push.
  HTTPS with a credential helper, or SSH, both work.
- An Epic Games account, for the Launcher.
- **Not needed yet:** EOS/Epic developer portal credentials. Online play runs on the Null services
  and the IP net driver until the EOS product exists (`PLAN/rfcs/needs-int-eos-config.md`).

---

## 2. One-time setup

### 2.1 Lay out the SSD

The project config expects this layout. The DDC path in `DefaultEngine.ini` is
`%GAMEDIR%../../../DDC`, which resolves to a `DDC` folder beside the clone.

```
/Volumes/Toshiba/Deepfield-Unreal/
├── deepfield-3d/     ← the clone (the Unreal working copy)
├── DDC/              ← Derived Data Cache, shared by every checkout on this machine
└── .editor-lock/     ← created and removed by Build/editor-lock.sh; never make it by hand
```

```bash
mkdir -p /Volumes/Toshiba/Deepfield-Unreal/DDC
```

### 2.2 Clone the `unreal/main` branch with LFS

```bash
cd /Volumes/Toshiba/Deepfield-Unreal
git lfs install                                   # once per machine; harmless to repeat
git clone --branch unreal/main https://github.com/cbhead/deep-field-3d.git deepfield-3d
cd deepfield-3d
```

**What you should see:** the clone finishes with a `Filtering content` line from LFS. The repo-root
`.lfsconfig` deliberately **skips** Megascans and the art/lighting sublevels on the Mac, because those
are GPU-box work, so a clone without them is correct.

**Check it:** these files should be real assets, not LFS pointer files:

```bash
git lfs ls-files | head        # lists .uasset/.umap files, each with a '*' (downloaded)
ls -lh unreal/DeepField/Content/DF/Dev/L_Dev_Empty.umap   # tens of KB or more, not ~130 bytes
```

### 2.3 Accept the Xcode licence and point at the full Xcode

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
```

### 2.4 Environment variables

Every script works with no variables set **if** you used the default paths above. If any path is
different, add these to `~/.zshrc` (with your own paths) and open a new terminal:

```bash
export UE_ROOT="/Users/Shared/Epic Games/UE_5.8"                         # the engine install
export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"       # full Xcode
export UE_LOCAL_DDC="/Volumes/Toshiba/Deepfield-Unreal/DDC"             # the DDC folder from §2.1
export EDITOR_LOCK_DIR="/Volumes/Toshiba/Deepfield-Unreal/.editor-lock" # one editor at a time
```

All the commands below are run **from the repository root** (`deepfield-3d/`).

---

## 3. Check the tree (seconds, no engine)

These are the Python checks CI runs. They need no engine, so run them first. They will catch a bad
clone before you spend minutes building.

```bash
python3 unreal/Build/layering-check.py           # → layering: OK
python3 unreal/Build/validate-content-json.py    # → one "ok <table>.json (N rows)" line per table
python3 unreal/Build/check-test-coverage.py      # → test coverage: 6 suite(s) registered, 6 gated, 0 ungated
python3 unreal/Build/plan-status.py --check      # → plan-status: OK (…)
```

All four should exit 0. A `plan-status` failure is only a stale ledger and does not stop you from building.

---

## 4. Build

The first build takes a long time: roughly 10–40 minutes on an M1, depending on what else the machine
is doing. Later builds are incremental.

```bash
"${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Build/BatchFiles/Mac/Build.sh" \
  DeepFieldEditor Mac Development \
  -Project="$PWD/unreal/DeepField/DeepField.uproject" \
  -WaitMutex -NoHotReload
```

**What you should see:** the build ends with `Result: Succeeded`. Everything above that line is
compiler output. If it fails, the first line containing ` error ` is the one to read:

```bash
# the same build, showing only errors and the result
".../Build.sh" DeepFieldEditor Mac Development -Project="$PWD/unreal/DeepField/DeepField.uproject" \
  -WaitMutex -NoHotReload 2>&1 | grep -E " error |Result:"
```

`-WaitMutex` makes a second build (another session's, or CI's) queue behind this one instead of
fighting it for memory.

---

## 5. Test

### 5.1 The landing gate (the full automated test set)

```bash
unreal/Build/editor-lock.sh unreal/Build/test.sh
```

- With no argument, this runs the whole landing gate (`DF_GATE_FILTER` in `Build/test.sh`: `DF.Unit`,
  `DF.Content`, `DF.Online`, `DF.Editor`, `DF.UI`, `DF.Func`). It is headless (`-nullrhi`) and takes a
  few minutes.
- **What you should see:** one `PASS <test path>` line per test, then `tests: OK — N passed (<log>)`. A failing
  test prints `FAIL <test path>` followed by its error lines, then `tests: FAILED — X of N`.
  **Exit code:** 0 means everything passed, 1 means a test failed or nothing matched, 2 means the editor
  did not finish.
- To run one area, pass a filter; `+` joins several:
  `unreal/Build/editor-lock.sh unreal/Build/test.sh DF.Unit.Match`.
- **Always go through `editor-lock.sh`.** It makes sure only one editor process runs on the machine
  at a time (an 8 GB Mac cannot hold two), and it points the editor at the shared DDC.
- `test.sh` **refuses to run** if any source file is newer than the built modules. Build again; do
  not bypass it. A green run on a stale binary looks exactly like a real pass.
- Logs: `unreal/DeepField/Saved/Logs/test-<filter>.log`. Report:
  `unreal/DeepField/Saved/Automation/Reports/test-<filter>/index.html`.

### 5.2 The map validator

```bash
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor-Cmd"
unreal/Build/editor-lock.sh "$UE" "$PWD/unreal/DeepField/DeepField.uproject" \
  -run=DFMapValidate -all -nullrhi -unattended -nop4 -nosplash -NoSound
```

**What you should see:** one `DFMapValidate <map>: N pass, 0 fail, …` line per map, and exit code 0.
Failures that are listed in `unreal/map-validation-baseline.tsv` show as warnings, not errors.

### 5.3 The network smoke test (a listen host and a client)

```bash
unreal/Build/editor-lock.sh unreal/Build/smoke-listen.sh
```

**What you should see:** `smoke: OK (host + 1 client(s) on /Game/DF/Dev/L_Dev_Empty)`. Add
`-client-count 2` to test two clients. Logs: `Saved/Logs/smoke-host.log` and `smoke-client-*.log`.

### 5.4 Everything at once (what CI's nightly runs)

```bash
unreal/Build/ci-local.sh          # checks → build → the gate → smoke; prints a summary table
```

It takes about 15–25 minutes when the machine is idle, and stops at the first failing step.

---

## 6. Open the editor and play

### 6.1 Open the editor

```bash
open -a "${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app" \
  --args "$PWD/unreal/DeepField/DeepField.uproject"
```

The first open compiles shaders into the DDC, which is slow once and fast afterwards. The editor
starts on `/Game/DF/Dev/L_Dev_Empty`, an empty test level.

### 6.2 Play a map

- **In the editor:** Content Browser → `Content/DF/Maps/<Map>/L_<Map>` (for example `L_Foundry`, or
  `L_Testlane` for the smallest one), open it, then press **Play**.
- **Standalone, from a terminal**, with match options on the URL:

  ```bash
  UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor-Cmd"
  "$UE" "$PWD/unreal/DeepField/DeepField.uproject" "/Game/DF/Maps/Testlane/L_Testlane?listen" -game -log
  ```

  | URL option | Effect |
  |---|---|
  | `?listen` | host a listen server (other machines join with `open <host-ip>`) |
  | `?endless` | endless mode: the waves never stop, and only the core ends the run |
  | `?lobby` | wait in a lobby until the lowest seat sends Launch |
  | `?intermission=<s>` | intermission length (default: the `intermissionSeconds` balance dial, 8 s) |
  | `?seed=<n>` | a fixed wave-plan seed |
  | `?wavesmap=<id>` | play another map's wave tables |

- **What works today** (2026-09-25): the maps are graybox imports of the Godot layouts, with no art
  yet. The match flow starts waves on the director, but **enemies are not spawned yet** (WS-05's
  `ADFEnemy` has not landed), so a wave never clears. `PLAN/NEXT.md` has the current state.

---

## 7. Everyday tasks

| Task | Command (from the repo root) |
|---|---|
| Re-import content after editing `unreal/content/json/*.json` | `python3 unreal/Build/validate-content-json.py`, then `unreal/Build/editor-lock.sh "$UE" "$PWD/unreal/DeepField/DeepField.uproject" -run=DFContentImport -nullrhi -unattended -nop4 -nosplash -NoSound`. Commit the JSON together with the regenerated `DT_*.uasset` files. Details: [content/README.md](content/README.md). |
| Re-import a level after editing `unreal/content/levels/*.level.json` | `… -run=DFLevelImport -map=<id> [-legacy] -nullrhi -unattended -nop4 -nosplash -NoSound`. Take the LFS lock first (`git lfs lock <path to the .umap>`). |
| Regenerate terrain after editing `unreal/content/terrain/*.terrain.json` | `python3 tools/ue-bridge/terrain/build_heightmap.py <map>`, then `… -run=DFEditor.DFTerrainImport -map=<map> …` (the `DFEditor.` prefix is required) |
| Check a workstream branch before opening a PR | `unreal/Build/pr-check.sh --ws <NN>` (see [Build/README.md](Build/README.md)) |
| Lock or unlock a binary asset you will edit | `git lfs lock <path>` / `git lfs unlock <path>`. Every `.uasset`/`.umap` is checked out read-only until you lock it (ADR-0010). |

Before claiming any work: read [PLAN/README.md](PLAN/README.md) (how sessions claim work and stay
aware of each other), `PLAN/STATUS.md` (who owns what) and `PLAN/NEXT.md` (what is actually finished
and what to pick up).

---

## 8. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `tests: no editor at …/UnrealEditor-Cmd (set UE_ROOT)` | The engine is not at the default path | Set `UE_ROOT` ([§2.4](#24-environment-variables)) |
| `xcrun: error` / the build cannot find a compiler | Only the Command Line Tools are selected, or the Xcode licence is not accepted | [§2.3](#23-accept-the-xcode-licence-and-point-at-the-full-xcode) |
| Assets fail to load; `.uasset` files are ~130 bytes | LFS pointers were checked out instead of the files | `git lfs install && git lfs pull` |
| `tests: REFUSING — <file> is newer than …` | The binary is older than the source | Build ([§4](#4-build)). Do **not** set `DF_TEST_ALLOW_STALE=1` to get past it. |
| `editor-lock: waiting for pid …` for a long time | Another session or CI holds the editor | Wait (up to 30 min). A lock whose process is gone is reclaimed automatically after 60 s. |
| `editor-lock: timed out` (exit 75) | The editor was busy for the whole timeout | Re-run it later |
| A test fails once, with a thread-lock assertion or a segfault | The Mac is out of memory, which makes verdicts wrong | Check `vm_stat` and close other apps. **One red is unproven: re-run once and say that you re-ran. Two reds are a real defect** (CONTRACTS/ci.md). |
| Files or commits vanish after an odd failure | The external SSD unmounted | Re-mount it, then compare `git log` with `git log origin/<branch>` before assuming anything |
| `Cannot remove … as it is read only` when saving an asset | LFS checks lockable assets out read-only | `git lfs lock <path>`. The importer commandlets clear the flag on exactly the files they write. |
| `-run=DFTerrainImport … could not find the class` | That commandlet's module loads late | Use `-run=DFEditor.DFTerrainImport` |
| Hundreds of engine tests run instead of ours | An automation filter is a case-insensitive **substring**, so `DF` matches engine test names | Spell out the roots (`DF.Unit+DF.Content`), or use `test.sh` with no argument |

Still stuck: the per-script details are in [Build/README.md](Build/README.md), and the hazards known
to every session are at the end of [PLAN/NEXT.md](PLAN/NEXT.md).

---

## Where things are

- **Start here for the programme:** [PLAN/README.md](PLAN/README.md) (how sessions claim work) and
  [PLAN/PROGRAMME.md](PLAN/PROGRAMME.md) (the plan of record).
- `PLAN/` — the ledger: status, ownership, decisions (ADRs), contracts, workstreams, RFCs, digests, NEXT.md.
- `content/` — the text source of truth for content numbers (`json/`), level files (`levels/`),
  terrain specs (`terrain/`) and their schemas.
- `Build/` — every script CI runs, each runnable by hand ([Build/README.md](Build/README.md)); the
  Windows bring-up checklist.
- `DeepField/` — the Unreal project (`DeepField.uproject`, `Config/`, `Source/` with 15 modules, `Content/`).
- `.gitattributes` (here) and `../.lfsconfig` (repo root) — the Git LFS rules for binaries (ADR-0010).
- `../tools/ue-bridge/` — the art and terrain tooling. `../tools/content-export/` — the one-time
  content bootstrap from the C# sim.
