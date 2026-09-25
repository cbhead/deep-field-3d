# Windows GPU workstation — bring-up runbook

**Status: written on the Mac, not yet run on the box.** Every command below comes from this
repository's config, from the engine's own requirement files in the UE 5.8.2 install, or from
Epic's and GitHub's documented tooling — but no step has been executed on Windows yet. Work
top to bottom, tick the boxes, and fix this file in the same PR as whatever turned out different.
The box is the machine ADR-0016 has been waiting for: until it builds, every render, perf,
Windows-packaging and Gauntlet lane in the ledger stays "unverified".

What the box is for (PROGRAMME.md §4.4, §7): Nanite / Lumen / VSM verification, the Megascans-heavy
`L_<Map>_Art` sublevels (fetch-excluded on the Mac), Windows packaging and the EOS overlay on
Windows, perf budgets (Appendix C§8), `DF.Soak.*`, Gauntlet, the Nanite-skeletal gate, and a shared
DDC. It is *not* a replacement for the Mac: the Mac stays the macOS build/package machine and the
8 GB memory floor.

## 0. Before you start

**Measure first.** `powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1`
(from PowerShell, not Git Bash) writes [`machines/windows-gpu.md`](machines/windows-gpu.md). Its
readiness table checks the box against every requirement in this file. Re-run it after each section
below and commit the result, so the next session reads what the box has instead of guessing.

- [ ] Windows 11 (or Windows 10 ≥ 19041 — the engine's `MinSoftwareVersion`), fully updated
      ([Windows 11 download](https://www.microsoft.com/software-download/windows11)).
- [ ] Current NVIDIA driver (Studio or Game Ready), installed clean
      ([NVIDIA drivers](https://www.nvidia.com/en-us/drivers/)). Hardware Lumen and the perf
      budgets are measured against it; write the driver version into your first session log.
- [ ] An NVMe volume with **≥ 500 GB free** for the clone, DDC, Intermediate and packages (the
      Mac's SSD budget was 60–120 GB *without* the art sublevels and Megascans; the box gets both).
      Below it is `D:\` — substitute your drive.
- [ ] **Short root path.** Use `D:\DF\`. Unreal and MSVC still trip over paths past 260 characters
      and External Actors (ADR-0006) generate long ones.
- [ ] Windows Security → Virus & threat protection → Exclusions: add `D:\DF\`, the engine install
      directory, and the processes `UnrealEditor.exe`, `UnrealEditor-Cmd.exe`, `ShaderCompileWorker.exe`,
      `UnrealBuildTool.exe`, `cl.exe`, `link.exe`. Real-time scanning of DDC and Intermediate writes
      is the single largest avoidable build cost on Windows.
- [ ] Settings → System → For developers → **enable long paths** (or
      `reg add HKLM\SYSTEM\CurrentControlSet\Control\FileSystem /v LongPathsEnabled /t REG_DWORD /d 1 /f`).

## 1. Engine — UE 5.8.2 from the launcher

No source build is needed at Level 1 (ADR-0003: listen server over EOS relay; the dedicated-server
target is the only thing a launcher build lacks, and that is the L2 seam).

- [ ] Install the [Epic Games Launcher](https://store.epicgames.com/download) (see also
      [unrealengine.com/download](https://www.unrealengine.com/download)), sign in with the account that owns the project's EGS/EOS
      organisation, install **Unreal Engine 5.8.2** — the same patch version as the Mac
      (`/Users/Shared/Epic Games/UE_5.8`, `EngineAssociation: "5.8"`). A different patch version
      re-saves assets on open; do not let that happen on a shared LFS repo.
- [ ] In the install options tick **Editor symbols for debugging** (crash callstacks are useless
      without them) and the **Windows** target platform. Nothing else is needed yet.
- [ ] Note the install path; below it is `%UE_ROOT%`, e.g. `C:\Program Files\Epic Games\UE_5.8`.
      Set it once: `setx UE_ROOT "C:\Program Files\Epic Games\UE_5.8"`.

## 2. Compiler — what *this* engine build asks for

Read from `Engine/Config/Windows/Windows_SDK.json` of the installed 5.8.2 (check the copy on the
box; it is authoritative if it differs):

| Requirement | Value |
|---|---|
| Preferred MSVC toolset | **14.44.35211 or later in the 14.44 family (VS 2022 17.14 LTSC)**, or 14.50.35723+ (VS 2026 18.x) |
| Banned MSVC | 14.39.x, 14.40–14.43, 14.44 below 35211, 14.50 below 35723 (compiler bugs the file names) |
| Minimum MSVC | 14.38.33130 |
| Windows SDK | **10.0.22621.0** preferred, 10.0.19041.0 minimum |
| Clang (optional, `clang-cl`) | 18.1.8 minimum, 20.1.x preferred |

Downloads:
[Visual Studio 2022 fixed-version installers (17.14)](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history) ·
[Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/older-downloads/) ·
[Windows SDK archive (10.0.22621)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/) ·
[LLVM / clang-cl releases](https://github.com/llvm/llvm-project/releases) ·
[.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0).

- [ ] Install **Visual Studio 2022 17.14** (Community is fine) with the workloads the engine
      suggests: *Desktop development with C++*, *Game development with C++*, *.NET desktop
      development*; and the individual components *MSVC v143 x64/x86 build tools (14.44, 17.14)*,
      *C++ ATL for 14.44*, *Windows 11 SDK 10.0.22621*, *.NET Framework 4.6.2 targeting pack*,
      *Unreal Engine IDE support* and *Unreal debugger*.
- [ ] If Visual Studio offers a newer default toolset, keep 14.44 installed side by side — UBT
      picks a preferred version when one is present and refuses a banned one.
- [ ] Do not install a separate .NET SDK for UBT: the engine bundles its own
      (`Engine/Binaries/ThirdParty/DotNet`). `tools/content-export` and `tools/waveplan-golden`
      want the **[.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0)** — install that one if you will run `content-export --diff` on the box.

## 3. Git, LFS and the clone

- [ ] Install [Git for Windows](https://git-scm.com/downloads/win) (includes
      [Git LFS](https://git-lfs.com)) and the [GitHub CLI](https://cli.github.com);
      `git lfs install`; `gh auth login`.
- [ ] Before cloning:
      ```bat
      git config --global core.longpaths true
      git config --global core.autocrlf false
      ```
      `autocrlf=false` matters: the repository has no root `.gitattributes`, the `unreal/Build/*.sh`
      scripts and every `unreal/content/json/*.json` are LF, and the join handshake hashes content
      (B§5 rule 9) — a CRLF checkout would make the box disagree with the Mac about the same commit.
- [ ] Clone into the short root, on the integration branch:
      ```bat
      mkdir D:\DF && cd /d D:\DF
      git clone --branch unreal/main https://github.com/cbhead/deep-field-3d.git deepfield-3d
      cd deepfield-3d
      ```
- [ ] **Undo the Mac's light clone.** `.lfsconfig` excludes Megascans, `L_*_Art*`, `L_*_Lighting*`
      and `Content/DF/Env/**` so the 8 GB Mac stays small (ADR-0010). The box is the machine those
      files are for:
      ```bat
      git config lfs.fetchexclude ""
      git lfs pull
      git lfs ls-files | find /c /v ""
      ```
      (Local git config wins over `.lfsconfig`; never edit `.lfsconfig` for this.) Today the
      excluded set is nearly empty — art workstreams have not landed — so this is cheap now and
      stays correct later.
- [ ] LFS locks work the same as on the Mac (`git lfs lock <file>` before editing any `.umap` or
      shared `.uasset`, PROGRAMME.md §6.4); `locksverify = true` is already in `.lfsconfig`.
- [ ] Megascans never come from git (ADR-0010): they are restored from [Fab](https://www.fab.com) by
      `tools/ue-bridge/ue/restore_fab.py` (WS-30) once that exists. Until then there is nothing to restore.
- [ ] Parallel sessions on the box use worktrees next to the clone, exactly like the Mac
      (`git worktree add D:\DF\wt-ws-NN -b ws/NN-<slug>/<topic> origin/unreal/main`). The box has the
      RAM for several editors; the two-slot rule of §6.7 is a Mac rule.

## 4. Derived Data Cache

`Config/DefaultEngine.ini` already points the local DDC at `%GAMEDIR%../../../DDC` — a sibling of
the clone — with the `UE-LocalDataCachePath` environment variable as the per-machine override. With
the clone at `D:\DF\deepfield-3d` that resolves to **`D:\DF\DDC`**, the same layout as the Mac's
`/Volumes/Toshiba/Deepfield-Unreal/DDC`. Nothing to configure.

- [ ] After the first editor launch confirm `D:\DF\DDC` exists and is filling, and that nothing
      appeared under `%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache`.

**Shared DDC (plan, not done).** The programme lists a shared DDC as blocked on this box. The
cheap version is an SMB share of a second directory on the box (`D:\DF\DDC-Shared`) added as the
`Shared` node of `[InstalledDerivedDataBackendGraph]` with its own env override, read-write on the
box and read-mostly from the Mac. That is an INT-owned ini change (`Config/Default*.ini`), it costs
the Mac LAN round-trips on every miss, and DDC entries are per-platform, so the Mac only gains what
is platform-independent. Raise it as a "Needs INT" when the box is building; do not improvise it.

## 5. First build

The `.sh` scripts in this directory are zsh and Mac-pathed; they do not run on Windows yet. The
python checks do. Until WS-15 ports the wrappers, run the engine tools directly:

- [ ] Python checks ([Python 3.9+](https://www.python.org/downloads/windows/), no packages):
      ```bat
      python unreal\Build\layering-check.py
      python unreal\Build\validate-content-json.py
      python unreal\Build\ownership-check.py --ws INT --base origin/unreal/main
      ```
- [ ] Editor target:
      ```bat
      "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" DeepFieldEditor Win64 Development ^
          -Project="D:\DF\deepfield-3d\unreal\DeepField\DeepField.uproject" -WaitMutex -NoHotReload
      ```
      Expect the first build to compile the shared PCHs and all 15 editor modules. Both targets use
      `BuildSettingsVersion.V7` (ADR-0020); if UBT complains about build settings, the engine on the
      box is not the same 5.8.2.
- [ ] Headless tests — the Mac's `test.sh`, spelled out:
      ```bat
      "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\DF\deepfield-3d\unreal\DeepField\DeepField.uproject" ^
          -nullrhi -unattended -nop4 -nosplash -NoSound ^
          -ExecCmds="Automation RunTests DF.Unit+DF.Content; Quit" ^
          -ReportExportPath="D:\DF\deepfield-3d\unreal\DeepField\Saved\Automation\Reports\first" -log
      ```
      The verdict is `Saved\Automation\Reports\first\index.json` (every test `Success`), not the exit
      code — the editor exits 0 whatever the tests did.
- [ ] Open the editor with a real RHI for the first time on any machine with headroom:
      `"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "...\DeepField.uproject"`. Load
      `/Game/DF/Dev/L_Dev_Empty`. Confirm in the output log: D3D12 / SM6, Nanite enabled, Lumen
      (hardware ray tracing available), Virtual Shadow Maps. ADR-0012's stack has never been seen
      rendering; write what you observe into the session log.
- [ ] Listen-host smoke by hand (what `smoke-listen.sh` does): one `UnrealEditor-Cmd.exe ... /Game/DF/Dev/L_Dev_Empty?listen -game -nullrhi -port=7788 -log`
      and one `... 127.0.0.1:7788 -game -nullrhi -log`; the client log must reach
      `Welcomed by server`.

## 6. Windows package

- [ ] Cook and package a Development build:
      ```bat
      "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
          -project="D:\DF\deepfield-3d\unreal\DeepField\DeepField.uproject" ^
          -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive ^
          -archivedirectory="D:\DF\packages\dev" -nop4 -utf8output -unattended
      ```
- [ ] Run `D:\DF\packages\dev\Windows\DeepField.exe`; it should reach `L_Dev_Empty` (or whatever
      `GameDefaultMap` is by then). A cook that fails on a `Placeholder=true` asset is the registry
      audit doing its job (WS-01), not a Windows problem.
- [ ] EOS on Windows (overlay, login, invite-only session over relay) is WS-11's verification, and
      it needs the portal credentials the human P0 list still shows as open. EGS upload
      (BuildPatchTool) is WS-15's packaging lane and comes after a package exists.
- [ ] A packaged Mac build can only be made on the Mac and a Windows one only here (no cross-cook of
      binaries); the box does not retire the Mac's packaging duty.

## 7. The floating-point check that only this box can run

`BuildSettingsVersion.V7` compiles **Editor** targets FP-precise but leaves **Game / Client /
Server** targets at Default, which is `/fp:fast` on MSVC (`CONTRACTS/ci.md`, GPU-box lane). Code that
must produce the same bits on every host pins its arithmetic by pragma — today that is
`Source/DFEnemies/{Public,Private}/Waves` (`FDFWavePlan`, `FDFDetMath`, `FDFDetRng`) — and those
pragmas have only ever been compiled by Apple clang.

- [ ] Use the **packaged** Development build from §6 — that is a Game target, and unlike a bare
      `Binaries\Win64\DeepField.exe` it has cooked content to start on. Run the golden tests in it:
      ```bat
      D:\DF\packages\dev\Windows\DeepField\Binaries\Win64\DeepField.exe -nullrhi -unattended ^
          -ExecCmds="Automation RunTests DF.Unit.WavePlan.; Quit" -log
      ```
      Development game builds compile the dev automation tests and Gauntlet drives them through the
      same console command, so this is expected to work — but it is the least certain command in
      this file. If the packaged build ignores it, say so here and hand the lane to WS-15 to run
      through Gauntlet instead.
      The trailing dot is deliberate: it selects the ten `DF.Unit.WavePlan.*` golden tests and
      leaves out `DF.Unit.WavePlanBaseline`, which reads `docs/gate-baseline.tsv` from the source
      tree. `PlansMatchSim` compares 237 plans bit for bit with the C# sim; a single failure there
      means `/fp:fast` got through the pragmas — file it against WS-05, do not loosen the test.
- [ ] Compare with the Editor-target run of the same filter from §5 and record both in the session log.

## 8. Self-hosted runner

`CONTRACTS/ci.md` has the GPU-box lane as "not yet: no box". The Mac runner's rules apply
unchanged — repository-level registration, a custom label so no other repository can land on the
machine, never PRs from forks — with one Windows-specific decision.

- [ ] GitHub → repository *Settings → Actions → Runners → New self-hosted runner → Windows / x64*;
      the page gives the download command (the zips are also on
      [actions/runner releases](https://github.com/actions/runner/releases)); unpack to
      `D:\actions-runner`, then:
      ```bat
      config.cmd --url https://github.com/cbhead/deep-field-3d --token <token> ^
          --name deepfield-gpu --labels deepfield,gpu --work _work --unattended
      ```
      `self-hosted`, `Windows` and `X64` are added automatically, so the lane's
      `runs-on: [self-hosted, Windows, X64, deepfield, gpu]`. The Mac is `deepfield` without `gpu`;
      keep both labels so a workflow can ask for "any project machine" or "the GPU box".
- [ ] **Service or interactive — decide per lane.** A Windows service runs in session 0 with no
      desktop. That is fine for builds, cooks, packaging and `-nullrhi` tests (`config.cmd
      --runasservice`). It is *not* a sound home for the lanes this box exists for — visual
      `DF.Vfx.EveryCueDraws`, `DF.Perf.<Map>`, Gauntlet with a real RHI — which want a logged-in
      desktop session on the GPU. For those, run the runner interactively (`run.cmd` from a
      scheduled task "at log on" of an auto-logon build account). WS-15 owns the choice; note which
      one you made in `CONTRACTS/ci.md`.
- [ ] Repository variables the workflows already read (`UE_ROOT`, `UE_LOCAL_DDC`, `EDITOR_LOCK_DIR`)
      are per-repository, not per-runner: the Windows lane will need its own names
      (`UE_ROOT_WIN`, …) or runner-level `.env` entries. Do not repoint the existing ones — the Mac
      nightly reads them.
- [ ] Runner workspace under `D:\actions-runner\_work` keeps CI's Intermediate and Binaries out of
      `D:\DF\deepfield-3d`; the DDC is shared, as on the Mac.
- [ ] The Windows workflow file itself (`.github/workflows/unreal-win.yml`) and the `.ps1` / `.bat`
      twins of `test.sh`, `smoke-listen.sh`, `pr-check.sh` and `ci-local.sh` do not exist yet. They
      are WS-15 work and the first thing worth doing on the box after this list is green.

## 9. When the list is green

- [ ] Append to `unreal/PLAN/DECISIONS.md` (INT numbers it): the box's spec, engine path, clone
      path, driver version, and the date ADR-0016's "Mac-only" constraint ended.
- [ ] Tell INT, so the digest can move lanes from "unverified" to "red/green" one at a time, and
      the workstreams marked `blocked: GPU box` (WS-37…42 budgets, WS-44, the Nanite-skeletal gate in
      WS-32/34) can be claimed.
- [ ] First real jobs for the box, in the order the programme needs them: a Windows package (G3 requires one) →
      the §7 FP check → `DF.Perf` baselines on `L_Dev_Empty` so the harness
      exists before a map does → hardware-Lumen / Nanite / VSM look-dev on the Foundry terrain as
      soon as WS-10a lands it.
