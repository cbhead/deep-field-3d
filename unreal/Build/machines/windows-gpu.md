# windows-gpu - machine inventory

_Generated 2026-09-26 17:34 -04:00 on `CHANDLER-ALIEN` by `unreal/Build/machine-inventory.ps1`._
_Do not edit by hand - re-run the script (`powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1`) and commit both this file and the `.json` beside it._

This is what the box **measurably has** at that instant, and how far it is from `windows-bringup.md`. 
If you are about to rely on something about this machine, check here first; if it is older than your last install, re-run the script. 20 bring-up checks, 2 not met or not knowable without elevation.

## Bring-up readiness

| Status | Item | Needs | This machine has | See |
|---|---|---|---|---|
| OK | Windows | Windows 11, or 10 >= 19041 | Microsoft Windows 11 Home build 26200.9457 | runbook 1.1 |
| OK | NVIDIA GPU + current driver | current Studio / Game Ready driver | NVIDIA GeForce RTX 5070, driver 610.60, 12227 MiB | runbook 1.1 |
| FAIL | Free NVMe space | >= 500 GB free on one volume | C: 359.4 GB free of 923.3 GB | windows-bringup 1 |
| WARN | Short clone root | <= 24 chars: D:\DF\deepfield-3d, or C:\DF\deepfield-3d without a D: drive (packaging is where long paths bite) | C:\Users\Cbhea\deep-field-3d (28 chars) | runbook 8 |
| OK | Long paths enabled | LongPathsEnabled = 1 | True | runbook 1.2 |
| OK | Defender exclusions | the clone, the DDC beside it, the engine, UnrealEditor/cl/link/... processes | C:\Program Files\Epic Games\UE_5.8; C:\Users\Cbhea\DDC; C:\Users\Cbhea\deep-field-3d; cl.exe; link.exe; ShaderCompileWorker.exe; UnrealBuildTool.exe; UnrealEditor-Cmd.exe; UnrealEditor.exe | runbook 1.4 |
| OK | Unreal Engine 5.8.3 | UE 5.8 from the launcher, the pinned patch 3 (deepfield.ps1 $EnginePatch): another patch re-saves assets on open | 5.8.3 (CL 58210709) at C:\Program Files\Epic Games\UE_5.8 | runbook 1.2 |
| OK | Editor symbols | the launcher option "Editor symbols for debugging" on the 5.8 install (about 60 GB): without it every engine frame of a crash reads UnknownFunction | installed in C:\Program Files\Epic Games\UE_5.8 | windows-bringup 1; runbook 1.2 |
| OK | UE_ROOT | set to the 5.8 install (deepfield finds the engine without it; the by-hand commands in runbook 2 use it) | C:\Program Files\Epic Games\UE_5.8 | runbook 2 |
| OK | MSVC toolset | 14.44 >= 35211 (VS 2022 17.14) or 14.50 >= 35723, judged by the compiler build (cl.exe), not the folder name | 14.44.35207 (compiler 14.44.35229) (preferred) | runbook 1.2 |
| OK | Windows SDK | 10.0.22621.0 (10.0.19041.0 minimum) | 10.0.22621.0, 10.0.26100.0 | runbook 1.2 |
| WARN | .NET 8 SDK | optional: only to regenerate the WavePlan goldens or re-export content from the sim (UBT uses its own bundled .NET) | 9.0.318 | tools/waveplan-golden |
| OK | Git for Windows | installed, on PATH | git version 2.55.0.windows.5 | runbook 1.2; ci.md runner 4 |
| OK | Git LFS | git lfs install done | git-lfs/3.7.1 (GitHub; windows amd64; go 1.25.1; git b84b3384); filter.lfs.process=git-lfs filter-process | runbook 1.2 |
| OK | core.longpaths | true | true | runbook 1.2 |
| OK | core.autocrlf | false | false | runbook 1.2 |
| OK | LFS light-clone undone | lfs.fetchexclude = "" in this clone | '' | runbook 1.2 |
| WARN | GitHub CLI | optional: gh auth login, to open PRs from the box (deepfield and the runner do not use it) | not installed | - |
| OK | Python 3.9+ | for the unreal\Build\*.py checks | Python 3.12.10 | runbook 3 |
| FAIL | Self-hosted runner | registered as deepfield-gpu with --labels deepfield | none | ci.md runner 1-8 |

`OK` meets the need; `WARN` works, or is optional, but is not what is asked for; `FAIL` missing or wrong; `UNKNOWN` needs an elevated run to read. "runbook N" is a section of unreal/README.md; "ci.md" is unreal/PLAN/CONTRACTS/ci.md.

## Hardware

| | |
|---|---|
| Machine | Alienware Alienware Aurora ACT1250 (BIOS 1.15.1 (2026-04-16)) |
| CPU | Intel(R) Core(TM) Ultra 7 265KF - 20 cores / 20 threads, max 3900 MHz, virtualization on (a hypervisor is running) |
| Memory | 31.7 GB usable; 2 of 2 slots filled: DIMM1 16 GB @ 5600 MT/s (80CE000080CE M323R2GA3EB0-CWMOL), DIMM2 16 GB @ 5600 MT/s (80CE000080CE M323R2GA3EB0-CWMOL) |
| Page file | C:\pagefile.sys 9216 MB |
| GPU | NVIDIA GeForce RTX 5070, 12227 MiB, compute 12.0, PCIe Gen5 x16, power limit 250.00 W |
| NVIDIA driver | 610.60 (CUDA -, VBIOS 98.05.36.40.30) |
| DirectX | DirectX 12; feature levels 12_2,12_1,12_0,11_1,11_0,10_1,10_0,9_3,9_2,9_1,1_0_CORE; WDDM 3.2 |
| HW GPU scheduling | default |
| Display adapter | NVIDIA GeForce RTX 5070 - driver 32.0.16.1060 (2026-06-08), 2560x1440 @ 59 Hz |
| Disk | NVMe BG7 KIOXIA 1024GB - NVMe SSD, 953.9 GB, Healthy, C: |
| Disk | TOSHIBA EXTERNAL_USB - USB Unspecified, 3726 GB, Healthy, no volume Windows can mount |
| Volume | C: 'OS' Fixed NTFS, 359.4 GB free of 923.3 GB |
| Network | Wi-Fi: Intel(R) Wi-Fi 7 BE200 320MHz, 1.2 Gbps |
| Power plan | Balanced |

## Operating system and settings

| | |
|---|---|
| Windows | Microsoft Windows 11 Home 25H2, build 26200.9457, 64-bit |
| Installed / last boot | 2026-09-19 / 2026-09-26 14:51 |
| Latest update | KB5124007 (2026-09-19) |
| Windows PowerShell | 5.1.26100.9444 |
| Long paths | True |
| Developer mode | False |
| Game mode | True |
| Defender real-time | True |
| Inventory ran elevated | True |

## Toolchain

| Tool | Version | On PATH | Path |
|---|---|---|---|
| git | git version 2.55.0.windows.5 | yes | C:\Program Files\Git\cmd\git.exe |
| git_lfs | git-lfs/3.7.1 (GitHub; windows amd64; go 1.25.1; git b84b3384) | yes | C:\Program Files\Git\cmd\git-lfs.exe |
| gh | not installed | - | - |
| python | Python 3.12.10 | yes | C:\Users\Cbhea\AppData\Local\Programs\Python\Python312\python.exe |
| dotnet | SDK 9.0.318 | yes | C:\Program Files\dotnet\dotnet.exe |
| node | not installed | - | - |
| cmake | not installed | - | - |
| clang_cl | not installed | - | - |
| blender | not installed | - | - |
| claude | not installed | - | - |
| vscode | 1.138.0 | yes | C:\Users\Cbhea\AppData\Local\Programs\Microsoft VS Code\bin\code.cmd |
| 7zip | not installed | - | - |

| .NET SDKs | 9.0.318 |
|---|---|
| Visual Studio | Visual Studio Community 2022 17.14.37710.0 - MSVC 14.44.35207 (compiler 14.44.35229) |
| Windows SDKs | 10.0.22621.0, 10.0.26100.0 |
| Epic Games Launcher | not installed |
| Unreal Engine | 5.8.3 (CL 58210709) at C:\Program Files\Epic Games\UE_5.8 |
| env UE_ROOT | C:\Program Files\Epic Games\UE_5.8 |
| env UE-LocalDataCachePath | - |
| env UE_LOCAL_DDC | - |
| env EDITOR_LOCK_DIR | - |
| git core.longpaths | true |
| git core.autocrlf | false |
| git filter.lfs.process | git-lfs filter-process |
| git user.name | cbhead |
| git lfs.fetchexclude (this clone) | - |

## This clone and CI

| | |
|---|---|
| Clone | C:\Users\Cbhea\deep-field-3d (28 chars) |
| Branch / HEAD at inventory | unreal/main / 5109e16 2026-09-26 machine-inventory: read git config as the clone sees it, wherever the script starts |
| Worktrees | 1 |
| Sibling DDC folder | True |
| Actions runner | none |

