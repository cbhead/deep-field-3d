# Deep Field 3D on Unreal Engine 5.8 — runbook

This is the Unreal rebuild of Deep Field 3D. It lives on the `unreal/main` branch, beside the frozen
Godot client (`../game/`) and the C# sim (`../sim/`, which is now the written spec). **All engine work
happens on Windows** (ADR-0028: the Mac is retired, and Windows is the only platform), and on Windows
one script, `Build/deepfield.ps1`, does the setup and the everyday jobs.

- **To set up a machine and play, build or test:** [§1](#1-set-up-and-run-one-script), and nothing
  else, is enough.
- **For the jobs the script does not do** (the map validator, packaging, the importers): §5.2, §6.3
  and §7, using the engine's own commands. §3–§5 also explain what `check`, `build`, `test`,
  `smoke`, `pr-check` and `ci-local` do and what they print.
- **Commissioning the GPU box** (first light, the first package, the floating-point check, the CI
  runner) is tracked in [Build/windows-bringup.md](Build/windows-bringup.md). What the box
  measurably has right now is in [Build/machines/windows-gpu.md](Build/machines/windows-gpu.md).

> **Status:** the script and this page have only partly been run on Windows. If a step turns out
> different, correct this file (or the script) in the same change as whatever you had to do. The zsh
> scripts in `Build/` (`test.sh`, `pr-check.sh`, …) were written for the retired Mac and do not run on
> Windows; `deepfield.ps1` and the commands below replace them.

---

## 1. Set up and run: one script

### 1.1 What you need before you start

Only these. The script installs everything else.

| What | Why |
|---|---|
| Windows 11, or Windows 10 version 2004 or later, 64-bit | Unreal Engine 5.8's minimum |
| An administrator account (you will approve a few UAC prompts) | Visual Studio, Git and long-path support install machine-wide |
| **About 250 GB free** (the engine ~60 GB, Visual Studio ~20 GB, the clone, its cache and build output 100+ GB) | The script warns below 150 GB on the drive it clones to. The GPU box needs more once the art sublevels and packages arrive; its checklist budgets 500 GB. |
| A free Epic Games account | Unreal Engine is installed through Epic's launcher, which needs a sign-in |
| A GPU with DirectX 12 and current drivers (NVIDIA, AMD or Intel Arc) | To open the editor or play; building and tests do not need one |
| Internet, and time: 1-3 hours the first time, mostly downloads | The engine is ~40 GB to download and Visual Studio ~20 GB |

### 1.2 Run it

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
| Repository | An existing clone anywhere on the machine (see pass 1; `-Dir` picks one when there are several) | Clones `unreal/main` to `D:\DF\deepfield-3d` (else `C:\DF\deepfield-3d`, or `-Dir`), turns off line-ending conversion, fetches every LFS file |
| Unreal Engine | The version `DeepField.uproject` names (5.8), ideally patch 5.8.2 | Installs the Epic Games Launcher and opens it. **This is the one manual step:** sign in, then Unreal Engine > Library > **+** next to *Engine versions* > **5.8.2** > Install. Press Enter in the script's window when it has finished. Sets `UE_ROOT` for you. |
| Visual Studio | VS 2022 (or 2026, or Build Tools) with an MSVC toolset the installed engine accepts (read from its `Engine\Config\Windows\Windows_SDK.json`; checked after the engine for that reason), and a Windows SDK 10.0.19041+ | Installs VS 2022 Community with the C++ game workloads, or updates and modifies the one you have. If it still has no accepted toolset, it prints the toolsets found and the engine's rules |
| Build | - | Builds the editor (10-30 minutes the first time) |

It ends with `setup: done`. From then on, in a terminal in the clone's `unreal\` folder (in PowerShell, type
`.\deepfield` instead of `deepfield`):

| Command | What it does |
|---|---|
| `deepfield play` | Builds if needed, then runs the game in a window. `-Map /Game/DF/Maps/Testlane/L_Testlane` plays another map ([§6.2](#62-play-a-map) lists what works today and the match options). |
| `deepfield host` / `deepfield join <ip>` | Host a game others on your network can join (port 7777), or join one. Allow UnrealEditor through Windows Firewall when asked. |
| `deepfield editor` | Builds if needed, then opens the Unreal editor. The first open compiles shaders: slow once, fast afterwards. |
| `deepfield test [filter]` | Builds, then runs the automated tests headless and prints PASS/FAIL per test. No filter = the landing gate from `Build/test.sh`. Example: `deepfield test DF.Unit.Tower`. |
| `deepfield pr-check` | Everything to run before opening a PR: the repository checks, your workstream's ownership check, the build, then the landing gate ([§5.4](#54-before-you-open-a-pr)). |
| `deepfield smoke` | Builds, then a headless listen host and client(s) that must join it ([§5.3](#53-the-network-smoke-test-a-listen-host-and-headless-clients)). |
| `deepfield ci-local` | Everything CI and a landing run, in order, with a summary table ([§5.5](#55-the-full-pre-merge-set)). |
| `deepfield check` | The repository checks that need no engine (layering, content schemas, test coverage). Seconds. |
| `deepfield build` | Only the build (DeepFieldEditor Win64 Development). |
| `deepfield solution` | Generates `DeepField.sln` for working on the C++ in Visual Studio or Rider. |
| `deepfield doctor` | Checks everything above and reports. Installs and changes nothing. |
| `deepfield help` | All commands and options. |

Logs: the build and test logs are in `unreal\DeepField\Saved\Logs\`, and a transcript of every
run is at `%LOCALAPPDATA%\DeepField\deepfield-<command>.log`.

### 1.3 If it stops

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

### 1.4 Once per machine, by hand: Defender exclusions

The script does not change antivirus settings. Real-time scanning of the DDC and Intermediate writes is
the largest avoidable build cost on Windows. In Windows Security → Virus & threat protection →
Exclusions, add the clone's parent folder (for example `D:\DF\`), the engine folder (`%UE_ROOT%`), and
the processes `UnrealEditor.exe`, `UnrealEditor-Cmd.exe`, `ShaderCompileWorker.exe`,
`UnrealBuildTool.exe`, `cl.exe` and `link.exe`.

**Why exactly 5.8.2:** a different engine patch version re-saves every asset it opens. On a shared LFS
repository that shows up as a wall of binary changes nobody meant to make. The script accepts any 5.8
the `.uproject` names but warns when the patch is not 2.

---

## 2. Commands by hand

§3–§7 run the engine directly, from **Command Prompt** (`cmd.exe`) in the repository root. cmd passes
engine arguments such as `-ExecCmds="…; Quit"` through verbatim; PowerShell's quoting rules differ.
`setup` sets `UE_ROOT`. Set these two shorthands in each new terminal:

```bat
set UE=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
set PROJ=%CD%\unreal\DeepField\DeepField.uproject
```

The DDC needs no configuration: `Config/DefaultEngine.ini` puts it at `%GAMEDIR%../../../DDC`, a `DDC`
folder beside the clone, shared by the clone and every worktree. The `UE-LocalDataCachePath`
environment variable overrides it if a machine needs another location.

---

## 3. Check the tree (seconds, no engine)

`deepfield check` runs the layering, content-schema and test-coverage checks. The ledger check it
leaves out is `python unreal\Build\plan-status.py --check` (expect `plan-status: OK (…)`). A
`plan-status` failure only means the ledger is stale; it does not stop you from building.

---

## 4. Build

`deepfield build` builds `DeepFieldEditor Win64 Development` and ends with `[ OK ] build succeeded`.
On a failure it prints the compiler errors in red, and the whole output is in
`unreal\DeepField\Saved\Logs\build-editor.log`. Underneath, it runs the engine's own script:

```bat
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" DeepFieldEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload
```

`-WaitMutex` makes a second build (another session's, or CI's) queue behind this one instead of
fighting it. Both targets use `BuildSettingsVersion.V7` (ADR-0020). If UBT complains about build
settings, the engine on the machine is not 5.8.

---

## 5. Test

`deepfield test` keeps the three rules `Build/test.sh` enforced. If you ever run the engine's
automation yourself, keep them by hand:

1. **Never test a binary older than its source.** `deepfield test` builds first and refuses to test
   modules older than the code (`-AllowStale` overrides it, loudly). A green run on a stale binary looks
   exactly like a real pass (CONTRACTS/ci.md, "A green run on a stale binary").
2. **The verdict is the JSON report, never the editor's exit code.** The editor exits 0 whatever the
   tests did.
3. **The landing gate is `DF_GATE_FILTER` in `Build/test.sh`**, the one definition every landing
   inherits. `deepfield test` with no filter reads it from there. An automation filter is a
   case-insensitive **substring** match, so plain `DF` also runs about 130 engine tests. Always spell the
   roots out.

### 5.1 The landing gate (the full automated test set)

```bat
unreal\deepfield test
```

It builds, then runs the gate headless (`-nullrhi`, a few minutes). **What you should see:** one `PASS
<test path>` line per test, then `[ OK ] N passed`. A failing test prints `FAIL <test path>` with its
error lines, then `X of N tests failed`. To run one area, pass a filter: `unreal\deepfield test
DF.Unit.Match` (`+` joins several). Log: `unreal\DeepField\Saved\Logs\test-<filter>.log`. Readable
report: `unreal\DeepField\Saved\Automation\Reports\test-<filter>\index.html`.

### 5.2 The map validator

```bat
"%UE%" "%PROJ%" -run=DFMapValidate -all -nullrhi -unattended -nop4 -nosplash -NoSound
```

**What you should see:** one `DFMapValidate <map>: N pass, 0 fail, …` line per map, and exit code 0.
Failures listed in `unreal/map-validation-baseline.tsv` show as warnings, not errors.

### 5.3 The network smoke test (a listen host and headless clients)

```bat
unreal\deepfield smoke
```

It builds, then starts a headless listen host on `/Game/DF/Dev/L_Dev_Empty` (port 7788, so it never
collides with a `deepfield host` on 7777) and one headless client that joins it. `-Clients 2` or `3`
adds more (the host takes one of the four seats), `-Map` picks another map and `-Port` another port.
Logs: `unreal\DeepField\Saved\Logs\smoke-host.log` and `smoke-client-<n>.log`.

**What you should see:** `[ OK ] host + 1 client(s) joined /Game/DF/Dev/L_Dev_Empty, seats 1, 2`. The
pass condition was re-derived against the join path PR #48 added, where `ADFGameMode::PreLogin` asks the
join validators before anyone is admitted:

- every client logs `Welcomed by server`, which the server sends only after `PreLogin` accepts. A
  client's `Bringing up level for play took` is not proof: a client that fails to connect loads its own
  default map and logs that too, which the old check accepted;
- the host logs `player joined` (`PostLogin`, after `PreLogin`) for its own player and every client,
  each with a seat from 1 to 4, never 0;
- the host logs no `join refused`. If it does, the smoke names the reason.

A bare `127.0.0.1` join is admitted through the online subsystem's dev-join branch, because a `?listen`
host is not a hosted session and there is no handshake to check. The smoke says when that happened.
The full handshake (version, content hash, approval) needs a hosted EOS session, which is WS-11's
verification.

`deepfield host` and `deepfield join <ip>` do the same with a real window, for playing rather than testing.

### 5.4 Before you open a PR

```bat
unreal\deepfield pr-check
```

It runs, in order: the repository checks and your workstream's ownership check, the build, then the
full landing gate, the same tests your branch will be landed on. The workstream comes from the branch
name (`ws/04-towers/rig` → `04`); on any other branch pass it: `deepfield pr-check -Ws 04`. `-Base`
changes the ref the ownership check diffs against (default `origin/unreal/main`).

**What you should see:** `pr-check: OK (WS-NN)`. It stops at the first failing step and says which. A
filter (`deepfield pr-check DF.Unit.Tower`) runs only those tests, for iterating, and ends
`pr-check: PARTIAL` rather than `OK`: run it without a filter before opening the PR. It does not run the
smoke, so add `deepfield smoke` (§5.3) if you touched anything networked.

In the ownership check, a binary outside your workstream's globs in `PLAN/OWNERSHIP.md` is a violation.
A text file outside them is a warning: open a PR to the owner, or write an RFC. Landing goes through INT, never the GitHub
merge button, because the button skips the only step that compiles (CONTRACTS/ci.md, 2026-09-25).

### 5.5 The full pre-merge set

```bat
unreal\deepfield ci-local
```

What the nightly lane runs, and what INT runs on a rebased branch before it lands (`Build/ci-local.sh`'s
job): layering, ownership in the INT view (`-Ws NN` for a workstream's), content schemas, test coverage,
the `STATUS.md` check, the build, the landing gate, then the smoke. It stops at the first failure and
prints a summary table either way, ending `ci-local: OK`.

- A stale `STATUS.md` is a `WARN`, not a failure: claims and lease renewals land on `unreal/main`
  between INT cycles by design. `-Strict` makes it fail.
- `-Skip smoke` (or `-Skip smoke,plan-status`) skips steps, and the verdict says which. A test filter
  ends the run `PARTIAL`, as with pr-check. `-Clients` and `-Port` go to the smoke.

---

## 6. Open the editor, play, package

### 6.1 Open the editor

`unreal\deepfield editor` builds if needed, then opens the editor. The first open compiles shaders into
the DDC, which is slow once and fast afterwards. The editor starts on `/Game/DF/Dev/L_Dev_Empty`, an
empty test level.

### 6.2 Play a map

- **In the editor:** Content Browser → `Content/DF/Maps/<Map>/L_<Map>` (for example `L_Foundry`, or
  `L_Testlane` for the smallest one), open it, then press **Play**.
- **From a terminal:** `unreal\deepfield play -Map /Game/DF/Maps/Testlane/L_Testlane`. Match options
  go on the map URL, for example `-Map "/Game/DF/Maps/Testlane/L_Testlane?endless?seed=7"`.
  `deepfield host` adds `?listen` itself.

  | URL option | Effect |
  |---|---|
  | `?listen` | host a listen server (other machines join with `open <host-ip>`, or `deepfield join <host-ip>`) |
  | `?endless` | endless mode: the waves never stop, and only the core ends the run |
  | `?lobby` | wait in a lobby until the lowest seat sends Launch |
  | `?intermission=<s>` | intermission length (default: the `intermissionSeconds` balance dial, 8 s) |
  | `?seed=<n>` | a fixed wave-plan seed |
  | `?wavesmap=<id>` | play another map's wave tables |

- **What works today** (2026-09-25): the maps are graybox imports of the Godot layouts, with no art
  yet. The match flow starts waves on the director, but **enemies are not spawned yet** (WS-05's
  `ADFEnemy` has not landed), so a wave never clears. `PLAN/NEXT.md` has the current state.

### 6.3 Package a Windows build

```bat
"%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" ^
    -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive ^
    -archivedirectory="%CD%\..\packages\dev" -nop4 -utf8output -unattended
```

The package lands in a `packages\dev` folder beside the clone. Run `packages\dev\Windows\DeepField.exe`
from there. It should reach `L_Dev_Empty` (or whatever `GameDefaultMap` is by then). A cook that fails
on a `Placeholder=true` asset is the registry audit doing its job (WS-01), not a packaging problem.

---

## 7. Everyday tasks

| Task | Command (from the repo root, with `UE` and `PROJ` set per §2) |
|---|---|
| Start a parallel session | `git worktree add ..\wt-ws-NN -b ws/NN-<slug>/<topic> origin/unreal/main`: a worktree beside the clone, sharing its DDC. Reset `PROJ` inside it. |
| Work on the C++ in Visual Studio or Rider | `unreal\deepfield solution` generates `DeepField.sln` |
| Re-import content after editing `unreal/content/json/*.json` | `python unreal\Build\validate-content-json.py`, then `"%UE%" "%PROJ%" -run=DFContentImport -nullrhi -unattended -nop4 -nosplash -NoSound`. Commit the JSON together with the regenerated `DT_*.uasset` files. Details: [content/README.md](content/README.md). |
| Re-import a level after editing its `.level.json` (`unreal/content/levels/<id>.level.json`, or today's Godot-era briefs in `levels/legacy/`, which `-legacy` selects) | Take the LFS lock first (`git lfs lock <path to the .umap>`), then `"%UE%" "%PROJ%" -run=DFLevelImport -map=<id> [-legacy] -nullrhi -unattended -nop4 -nosplash -NoSound`. |
| Regenerate terrain after editing `unreal/content/terrain/*.terrain.json` | `python tools\ue-bridge\terrain\build_heightmap.py <map>`, then `"%UE%" "%PROJ%" -run=DFEditor.DFTerrainImport -map=<map> -nullrhi -unattended -nop4 -nosplash -NoSound`. The `DFEditor.` prefix is required. |
| Lock or unlock a binary asset you will edit | `git lfs lock <path>` / `git lfs unlock <path>`. Every `.uasset`/`.umap` is checked out read-only until you lock it (ADR-0010). |
| Record what this machine has, after installing anything | `powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1`, then commit `Build/machines/<name>.md` and `.json` ([Build/machines/README.md](Build/machines/README.md)) |

Before claiming any work, read [PLAN/README.md](PLAN/README.md) (how sessions claim work and stay
aware of each other), `PLAN/STATUS.md` (who owns what) and `PLAN/NEXT.md` (what is actually finished
and what to pick up).

---

## 8. Troubleshooting

Setup problems are in [§1.3](#13-if-it-stops); `deepfield doctor` re-checks everything without
changing anything.

| Symptom | Cause | Fix |
|---|---|---|
| `The system cannot find the path specified` for `%UE%` or `Build.bat` | `UE_ROOT` is not set in this terminal | Open a new terminal after `setup`, or `setx UE_ROOT "<engine folder>"` |
| UBT complains about build settings | The engine is not 5.8 | Install 5.8.2 exactly |
| Errors about paths longer than 260 characters | The clone is too deep | Keep the clone at a short root (`D:\DF\deepfield-3d`, the script's default) |
| Builds and the first editor open are very slow | Defender is scanning the DDC and Intermediate writes | [§1.4](#14-once-per-machine-by-hand-defender-exclusions) |
| `... is newer than the built modules` from `deepfield test` | The build before the tests failed, so the binary is stale | Fix the build; never pass `-AllowStale` to get past it |
| A test fails once, with an assertion or a crash unrelated to the change | Unproven | **Re-run once and say that you re-ran. Two reds are a real defect** (CONTRACTS/ci.md). |
| `Cannot remove … as it is read only` when saving an asset | LFS checks lockable assets out read-only | `git lfs lock <path>`. The importer commandlets clear the flag on exactly the files they write. |
| `-run=DFTerrainImport … could not find the class` | That commandlet's module loads late | Use `-run=DFEditor.DFTerrainImport` |
| Hundreds of engine tests run instead of ours | An automation filter is a case-insensitive **substring**, so `DF` matches engine test names | Spell out the roots, as in [§5](#5-test) |
| The DDC fills `%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache` instead of a `DDC` folder beside the clone | `UE-LocalDataCachePath` points elsewhere, or the project is not at `<clone>\unreal\DeepField` | See [§2](#2-commands-by-hand) |

Still stuck: the per-script details are in [Build/README.md](Build/README.md), and the hazards known
to every session are in [PLAN/NEXT.md](PLAN/NEXT.md).

---

## Where things are

- **Start here for the programme:** [PLAN/README.md](PLAN/README.md) (how sessions claim work) and
  [PLAN/PROGRAMME.md](PLAN/PROGRAMME.md) (the plan of record).
- `PLAN/` — the ledger: status, ownership, decisions (ADRs), contracts, workstreams, RFCs, digests, NEXT.md.
- `content/` — the text source of truth for content numbers (`json/`), level files (`levels/`),
  terrain specs (`terrain/`) and their schemas.
- `deepfield.cmd` and `Build/deepfield.ps1` — the Windows script. `Build/` also holds the CI checks
  ([Build/README.md](Build/README.md)), the GPU-box commissioning checklist and the machine inventories.
- `DeepField/` — the Unreal project (`DeepField.uproject`, `Config/`, `Source/` with 15 modules, `Content/`).
- `.gitattributes` (here) and `../.lfsconfig` (repo root) — the Git LFS rules for binaries (ADR-0010).
- `../tools/ue-bridge/` — the terrain tooling today, and the art converter as WS-30 builds it.
  `../tools/content-export/` — the one-time content bootstrap from the C# sim.
