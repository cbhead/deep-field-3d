# Deep Field 3D on Unreal Engine 5.8 — runbook

This is the Unreal rebuild of Deep Field 3D. It lives on the `unreal/main` branch, beside the frozen
Godot client (`../game/`) and the C# sim (`../sim/`, which is now the written spec).

- **On Windows** (a developer, a tester or a player): one script does everything. It checks the
  machine, installs what is missing, clones the repository, builds, and runs the game. Follow
  [§0 Windows](#0-windows-one-script) and nothing else on this page.
- **On the Mac:** [§1](#1-prerequisites) onwards takes you from a bare Mac to a built project with its
  tests passing, then to playing a map. Do the sections in order; each step says what to run and what
  you should see. If you see something else, look it up in [Troubleshooting](#8-troubleshooting).

Registering the CI runner is covered in [PLAN/CONTRACTS/ci.md](PLAN/CONTRACTS/ci.md). The GPU
workstation's extra checks (Nanite/Lumen, packaging, the floating-point check) are in
[Build/windows-bringup.md](Build/windows-bringup.md).

> Every command here comes from this repository's own scripts and config (`Build/*.sh`,
> `Build/deepfield.ps1`, `.lfsconfig`, `DeepField/Config/DefaultEngine.ini`). If a step turns out
> different on your machine, correct this file in the same change as whatever you had to do.

---

## 0. Windows: one script

### 0.1 What you need before you start

Only these. The script installs everything else.

| What | Why |
|---|---|
| Windows 11, or Windows 10 version 2004 or later, 64-bit | Unreal Engine 5.8's minimum |
| An administrator account (you will approve a few UAC prompts) | Visual Studio, Git and long-path support install machine-wide |
| **About 250 GB free** (the engine ~60 GB, Visual Studio ~20 GB, the clone, its cache and build output 100+ GB) | The script warns below 150 GB on the drive it clones to |
| A free Epic Games account | Unreal Engine is installed through Epic's launcher, which needs a sign-in |
| A GPU with DirectX 12 and current drivers (NVIDIA, AMD or Intel Arc) | To open the editor or play; building and tests do not need one |
| Internet, and time: 1-3 hours the first time, mostly downloads | The engine is ~40 GB and Visual Studio ~20 GB |

### 0.2 Run it

**On a machine that does not have the repository yet**, open **PowerShell** (Start menu, type
`PowerShell`, Enter; the ordinary window, not "as administrator") and paste this one line:

```powershell
irm https://raw.githubusercontent.com/cbhead/deep-field-3d/unreal/main/unreal/Build/deepfield.ps1 -OutFile "$env:TEMP\deepfield.ps1"; powershell -NoProfile -ExecutionPolicy Bypass -File "$env:TEMP\deepfield.ps1" setup
```

**If you already have a clone**, double-click `unreal\deepfield.cmd` in it, or run `unreal\deepfield setup`
in a terminal.

`setup` never assumes a bare machine. It works in two passes:

1. **Check.** It looks for everything below and changes nothing. That includes things installed
   somewhere unusual: Git or Python that isn't on PATH (including GitHub Desktop's Git), any Visual
   Studio 2022/2026 or Build Tools, the engine wherever the launcher put it (or a source build), and
   an existing clone. For the clone it checks the folder you're in, the last clone it used, `X:\DF\deepfield-3d` on
   every drive, the usual folders under your user profile, and then searches three folders deep on each
   drive. It then prints a summary, *already in place: N* and *missing: …*, and asks **Go ahead? [Y/n]**.
2. **Fix.** Only the items listed as missing are installed or changed. Everything marked OK is left
   alone. `-Yes` skips the question.

`deepfield doctor` runs pass 1 alone. Running `setup` again is safe, and it is also how you continue after stopping.

| Step | What it checks | What it does if it is missing |
|---|---|---|
| Windows | 64-bit, build 19041+; long paths enabled | Enables long paths (one UAC prompt) |
| Git | Git for Windows and Git LFS | Installs them with winget |
| Python | Python 3.9+ (the Store's fake `python.exe` does not count) | Installs Python 3.12 with winget |
| Visual Studio | VS 2022 (or 2026) with an MSVC toolset UE 5.8 accepts (14.44.35211+, not a banned one), and a Windows SDK 10.0.19041+ | Installs VS 2022 Community with the C++ game workloads, or updates and modifies the one you have |
| Repository | An existing clone anywhere on the machine (see pass 1; `-Dir` picks one when there are several) | Clones `unreal/main` to `D:\DF\deepfield-3d` (else `C:\DF\deepfield-3d`, or `-Dir`), turns off line-ending conversion, fetches every LFS file |
| Unreal Engine | The version `DeepField.uproject` names (5.8), ideally patch 5.8.2 | Installs the Epic Games Launcher and opens it. **This is the one manual step:** sign in, then Unreal Engine > Library > **+** next to *Engine versions* > **5.8.2** > Install. Press Enter in the script's window when it has finished. Sets `UE_ROOT` for you. |
| Build | - | Builds the editor (10-30 minutes the first time) |

It ends with `setup: done`. From then on, in a terminal in the clone's `unreal\` folder (in PowerShell, type
`.\deepfield` instead of `deepfield`):

| Command | What it does |
|---|---|
| `deepfield play` | Builds if needed, then runs the game in a window. `-Map /Game/DF/Maps/Testlane/L_Testlane` plays another map ([§6.2](#62-play-a-map) lists what works today). |
| `deepfield host` / `deepfield join <ip>` | Host a game others on your network can join (port 7777), or join one. Allow UnrealEditor through Windows Firewall when asked. |
| `deepfield editor` | Builds if needed, then opens the Unreal editor. The first open compiles shaders: slow once, fast afterwards. |
| `deepfield test [filter]` | Builds, then runs the automated tests headless and prints PASS/FAIL per test. No filter = the landing gate from `Build/test.sh`. Example: `deepfield test DF.Unit.Tower`. |
| `deepfield check` | The repository checks that need no engine (layering, content schemas, test coverage). Seconds. |
| `deepfield build` | Only the build (DeepFieldEditor Win64 Development). |
| `deepfield solution` | Generates `DeepField.sln` for working on the C++ in Visual Studio or Rider. |
| `deepfield doctor` | Checks everything above and reports. Installs and changes nothing. |
| `deepfield help` | All commands and options. |

Logs: the build and test logs are in `unreal\DeepField\Saved\Logs\`, and a transcript of every
run is at `%LOCALAPPDATA%\DeepField\deepfield-<command>.log`.

### 0.3 If it stops

It stops at the first thing it cannot fix itself and says what to do, in red. Do that, then run the
same command again; everything done so far is kept.

| It says | Do this |
|---|---|
| `winget is not available` | Microsoft Store > search **App Installer** > Install/Update, then run it again. |
| `Unreal Engine 5.8 is not installed yet` (after you typed Q) | Finish the install in the Epic Games Launcher, then `deepfield setup`. |
| `not found yet` although the launcher shows 5.8 installed | It is in an unusual place: `deepfield setup -EngineDir "E:\Epic\UE_5.8"` (the folder that contains `Engine\`). |
| `no MSVC toolset UE 5.8 accepts` | Visual Studio Installer > Update, then Modify > Individual components > **MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)**. |
| `the build failed` | The compiler errors are printed in red above it. Paste them to whoever owns the code, or into a Claude session. |
| `... is newer than the built modules` (tests) | The source changed after the last build. `deepfield test` builds first, so this only appears when that build failed. |
| `the clone converts line endings, and there are uncommitted changes` | Commit or stash your changes, then run `deepfield setup` again. |
| Anything marked `Unexpected error` | A bug in the script. Report it with the transcript it names. |

---

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
| Xcode | 26.6 (full Xcode, not only the Command Line Tools) | [Mac App Store](https://apps.apple.com/app/xcode/id497799835) (older versions: [developer.apple.com/download/all](https://developer.apple.com/download/all/?q=xcode)), then run `sudo xcodebuild -license accept` once | `xcodebuild -version` |
| Epic Games Launcher | current | [store.epicgames.com/download](https://store.epicgames.com/download) (see also [unrealengine.com/download](https://www.unrealengine.com/download)) | — |
| **Unreal Engine** | **5.8.2** exactly | Launcher → Unreal Engine → Library → **+** → 5.8.2. Keep the default location `/Users/Shared/Epic Games/UE_5.8`. | `ls "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"` |
| Homebrew | current | [brew.sh](https://brew.sh) | `brew --version` |
| Git | 2.40+ | `brew install git` (or Xcode's); [git-scm.com/downloads/mac](https://git-scm.com/downloads/mac) | `git --version` |
| **Git LFS** | 3.x | `brew install git-lfs` ([git-lfs.com](https://git-lfs.com)), then **`git lfs install`** once | `git lfs version` |
| Python | 3.9+ (the system `python3` is fine; no packages needed) | built in; otherwise [python.org/downloads/macos](https://www.python.org/downloads/macos/) | `python3 --version` |
| zsh | the macOS default shell | built in | `zsh --version` |
| .NET SDK 8 | *optional*; only for `tools/content-export --diff` | `brew install --cask dotnet-sdk`, or [dotnet.microsoft.com/download/dotnet/8.0](https://dotnet.microsoft.com/en-us/download/dotnet/8.0) | `dotnet --version` |

**Why exactly 5.8.2:** a different engine patch version re-saves every asset it opens. On a shared LFS
repository that shows up as a wall of binary changes nobody meant to make.

### 1.3 Accounts and access

- A GitHub account with read access to `cbhead/deep-field-3d`, plus write access if you will push.
  HTTPS with a credential helper, or SSH, both work.
- An [Epic Games account](https://www.epicgames.com/id/register), for the Launcher.
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
