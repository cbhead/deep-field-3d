# Windows GPU workstation — commissioning checklist

**Status: in progress.** The box exists (ADR-0023) and is the only engine machine (ADR-0028). Setting
it up as a development machine is `unreal\deepfield.cmd setup` ([the runbook](../README.md), §1). This
file tracks what is left to *prove* on it: the first results that move the ledger's render, perf,
packaging and CI lanes from "unverified" to red or green. Owning the hardware is not the same as having
measured anything on it.

**Measure first, and after every step.** `powershell -ExecutionPolicy Bypass -File
unreal\Build\machine-inventory.ps1` (from PowerShell or cmd, not Git Bash) writes
[`machines/windows-gpu.md`](machines/windows-gpu.md). Its readiness table checks the box against this
file. Re-run it and commit the result after each section, so the next session reads what the box has
instead of guessing. At the last inventory (2026-09-24), the engine, Visual Studio, the Windows SDK and
the .NET 8 SDK were not installed yet, the clone was at `C:\Users\Cbhea\deep-field-3d`, and the only
volume (C:) had 181 GB free.

Tick boxes in a PR as you go, write what you observed into your session log, and if a runbook step or
the script turned out different, fix it in the same PR.

## 1. The box builds and tests

- [ ] `unreal\deepfield.cmd setup` ends with `setup: done`: Git and LFS, Python, Visual Studio with an
      accepted MSVC toolset, a Windows SDK, UE 5.8.2 (tick **Editor symbols for debugging** in the
      launcher's install options, because crash callstacks are useless without them), the clone with
      every LFS file, and the first build. The Defender exclusions (runbook §1.4) are done.
- [ ] Disk: the drive the clone is on has **≥ 500 GB free** once the art sublevels, Megascans and
      packages arrive. At the last inventory, C: had 181 GB. Plan the space before the art lanes start.
- [ ] `machine-inventory.ps1` re-run and committed. Its readiness table shows no `FAIL` except the
      runner (§5), and has an elevated run behind it so that the Defender row is not `UNKNOWN`.
- [ ] Runbook §3 checks pass, and §5.1 (`deepfield test`) passes the landing gate. This is the suite's
      first MSVC verdict, since every earlier run was Apple clang on the retired Mac. Record the count.
      It is also the first build of PR #51, which has never been compiled (PLAN/NEXT.md).
- [ ] Runbook §5.2 (map validator) and §5.3 (listen-host smoke) pass, with the smoke's pass condition
      re-derived first (runbook §5.3).

## 2. First light with a real RHI

- [ ] `deepfield editor` (runbook §6.1), and load `/Game/DF/Dev/L_Dev_Empty`. Confirm in the output log:
      D3D12 / SM6, Nanite enabled, Lumen with hardware ray tracing available, Virtual Shadow Maps.
      ADR-0012's stack has never been seen rendering, so write what you observe into the session log.
- [ ] A `DDC` folder beside the clone exists and is filling; nothing appeared under
      `%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache`.

## 3. First Windows package

- [ ] Package a Development build (runbook §6.3) and run it. It should reach `L_Dev_Empty` (or
      whatever `GameDefaultMap` is by then).
- [ ] EOS on Windows (overlay, login, an invite-only session over relay) is WS-11's verification. It
      needs the portal credentials the human P0 list still shows as open. The EGS upload
      (BuildPatchTool) is WS-15's packaging lane and comes after a package exists.

## 4. The floating-point check on a Game target

`BuildSettingsVersion.V7` compiles **Editor** targets FP-precise but leaves **Game / Client / Server**
targets at Default, which is `/fp:fast` on MSVC (CONTRACTS/ci.md, "GPU-box lane"). Code that must
produce the same bits everywhere pins its arithmetic with the `DF_DET_FP_*` pragmas in
`Source/DFCore/Public/Determinism/` (`DFDetMath.h`, `DFDetRng.h`), used by
`DFEnemies/Private/Waves/DFWavePlan.cpp`, `DFEnemies/Private/Movement/DFLaneWalker.cpp` and
`DFTowers/Private/Towers/DFTowerMath.cpp`. Those pragmas have only ever been compiled by clang.

- [ ] Use the **packaged** Development build from §3. That is a Game target, and unlike a bare
      `Binaries\Win64\DeepField.exe` it has cooked content to start on. Run the golden tests in it,
      from the folder the package landed in (`packages\dev` beside the clone):
      ```bat
      Windows\DeepField\Binaries\Win64\DeepField.exe -nullrhi -unattended ^
          -ExecCmds="Automation RunTests DF.Unit.WavePlan.; Quit" -log
      ```
      Development game builds compile the dev automation tests, and Gauntlet drives them through the
      same console command, so this is expected to work. It is also the least certain command in this
      file: if the packaged build ignores it, say so here and hand the lane to WS-15 to run through
      Gauntlet instead.
      The trailing dot is deliberate. It selects the 13 `DF.Unit.WavePlan.*` golden tests and leaves
      out `DF.Unit.WavePlanBaseline`, which reads `docs/gate-baseline.tsv` from the source tree.
      `PlansMatchSim` compares 237 plans bit for bit with the C# sim. A single failure there means
      `/fp:fast` got through the pragmas: file it against WS-05, and do not loosen the test.
      Tower math (`DF.Unit.Tower`, WS-04) is pinned the same way; add `+DF.Unit.Tower` to the filter
      once that suite has passed an Editor-target run.
- [ ] Compare with the Editor-target run of the same filter (`deepfield test DF.Unit.WavePlan.`) and
      record both in the session log.

## 5. The self-hosted runner

- [ ] Register the box as the repository's runner, following CONTRACTS/ci.md, "The GPU box as the
      self-hosted runner".
- [ ] Decide **service or interactive** for each lane, and note the choice in `CONTRACTS/ci.md`
      (WS-15 owns it).
- [ ] The Windows workflow, and the ports of `pr-check.sh`, `ci-local.sh` and `int-merge.sh` onto
      `deepfield.ps1`, are WS-15 work and the first thing worth doing once this section is green. Until
      they exist, GitHub proves nothing about the Unreal tree beyond the hosted Python checks. Once the
      runner's check is green, requiring it on `unreal/main` through branch protection is the owner's
      call (CONTRACTS/ci.md, 2026-09-25).

## 6. When the list is green

- [ ] Record the box's spec, driver, engine and clone paths in the digest, and in `PLAN/NEXT.md`
      ("The machine"). The inventory file has the facts.
- [ ] Tell INT, so that the digest can move lanes from "unverified" to red or green one at a time, and
      so that the work that needs a measured box can be claimed (the WS-37…42 budgets, WS-44, and the
      Nanite-skeletal gate in WS-32/34).
- [ ] The first real jobs, in the order the programme needs them: a Windows package (G2 requires one)
      → the §4 FP check → `DF.Perf` baselines on `L_Dev_Empty`, so the harness exists before a map
      does → hardware Lumen / Nanite / VSM look-dev on the Foundry terrain as soon as WS-10a lands it.
