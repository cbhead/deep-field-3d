# windows-gpu - machine inventory

_Generated 2026-09-24 23:37 -04:00 on `CHANDLER-ALIEN` by `unreal/Build/machine-inventory.ps1`._
_Do not edit by hand - re-run the script (`powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1`) and commit both this file and the `.json` beside it._

This is what the box **measurably has** at that instant, and how far it is from `windows-bringup.md`. 
If you are about to rely on something about this machine, check here first; if it is older than your last install, re-run the script. 19 bring-up checks, 12 not met or not knowable without elevation.

## Bring-up readiness

| Status | Item | Runbook needs | This machine has | Runbook section |
|---|---|---|---|---|
| OK | Windows | Windows 11, or 10 >= 19041 | Microsoft Windows 11 Home build 26200.9457 | 0 |
| OK | NVIDIA GPU + current driver | current Studio / Game Ready driver | NVIDIA GeForce RTX 5070, driver 581.04, 12227 MiB | 0 |
| FAIL | Free NVMe space | >= 500 GB free on one volume | C: 181.2 GB free of 923.3 GB | 0 |
| WARN | Short clone root | D:\DF\deepfield-3d (short path) | C:\Users\Cbhea\deep-field-3d (28 chars) | 0, 3 |
| OK | Long paths enabled | LongPathsEnabled = 1 | True | 0 |
| UNKNOWN | Defender exclusions | clone, engine, UnrealEditor/cl/link/... processes | unknown (run elevated to read) | 0 |
| FAIL | Unreal Engine 5.8.2 | UE 5.8.2 from the launcher (same patch as the Mac) | none installed | 1 |
| FAIL | UE_ROOT | set to the 5.8 install | unset | 1 |
| FAIL | MSVC toolset | 14.44 >= 35211 (VS 2022 17.14) or 14.50 >= 35723 | no Visual Studio / MSVC | 2 |
| FAIL | Windows SDK | 10.0.22621.0 (10.0.19041.0 minimum) | none | 2 |
| FAIL | .NET 8 SDK | for tools/content-export, waveplan-golden | no SDK | 2 |
| WARN | Git for Windows | installed, on PATH | git version 2.55.0.windows.5 - NOT on PATH | 3 |
| OK | Git LFS | git lfs install done | git-lfs/3.7.1 (GitHub; windows amd64; go 1.25.1; git b84b3384); filter.lfs.process=git-lfs filter-process | 3 |
| FAIL | core.longpaths | true | - | 3 |
| FAIL | core.autocrlf | false | true | 3 |
| FAIL | LFS light-clone undone | lfs.fetchexclude = "" in this clone | unset (the .lfsconfig Mac excludes apply) | 3 |
| FAIL | GitHub CLI | installed, gh auth login | not installed | 3 |
| OK | Python 3.9+ | for the unreal\Build\*.py checks | Python 3.12.10 | 5 |
| FAIL | Self-hosted runner | deepfield-gpu, labels deepfield,gpu | none | 8 |

`OK` meets the runbook; `WARN` works but is not what the runbook asks for; `FAIL` missing or wrong; `UNKNOWN` needs an elevated run to read.

## Hardware

| | |
|---|---|
| Machine | Alienware Alienware Aurora ACT1250 (BIOS 1.15.1 (2026-04-16)) |
| CPU | Intel(R) Core(TM) Ultra 7 265KF - 20 cores / 20 threads, max 3900 MHz, virtualization on (a hypervisor is running) |
| Memory | 31.7 GB usable; 2 of 2 slots filled: DIMM1 16 GB @ 5600 MT/s (80CE000080CE M323R2GA3EB0-CWMOL), DIMM2 16 GB @ 5600 MT/s (80CE000080CE M323R2GA3EB0-CWMOL) |
| Page file | C:\pagefile.sys 5968 MB |
| GPU | NVIDIA GeForce RTX 5070, 12227 MiB, compute 12.0, PCIe Gen5 x16, power limit 250.00 W |
| NVIDIA driver | 581.04 (CUDA 13.0, VBIOS 98.05.36.40.30) |
| DirectX | DirectX 12; feature levels 12_2,12_1,12_0,11_1,11_0,10_1,10_0,9_3,9_2,9_1,1_0_CORE; WDDM 3.2 |
| HW GPU scheduling | default |
| Display adapter | DisplayLink USB Device - driver 9.3.3324.0 (2020-04-15), 1920x1080 @ 60 Hz |
| Display adapter | DisplayLink USB Device - driver 9.3.3324.0 (2020-04-15), 1920x1080 @ 60 Hz |
| Display adapter | DisplayLink USB Device - driver 9.3.3324.0 (2020-04-15) |
| Display adapter | NVIDIA GeForce RTX 5070 - driver 32.0.15.8104 (2025-08-13), 1920x1080 @ 60 Hz |
| Display adapter | DisplayLink USB Device - driver 9.3.3324.0 (2020-04-15) |
| Disk | NVMe BG7 KIOXIA 1024GB - NVMe SSD, 953.9 GB, Healthy, C: |
| Disk | TOSHIBA EXTERNAL_USB - USB Unspecified, 3726 GB, Healthy, no volume Windows can mount |
| Volume | C: 'OS' Fixed NTFS, 181.2 GB free of 923.3 GB |
| Network | Wi-Fi: Intel(R) Wi-Fi 7 BE200 320MHz, 1.2 Gbps |
| Power plan | Balanced |

## Operating system and settings

| | |
|---|---|
| Windows | Microsoft Windows 11 Home 25H2, build 26200.9457, 64-bit |
| Installed / last boot | 2026-09-19 / 2026-09-20 10:22 |
| Latest update | KB5124007 (2026-09-19) |
| Windows PowerShell | 5.1.26100.9444 |
| Long paths | True |
| Developer mode | False |
| Game mode | True |
| Defender real-time | True |
| Inventory ran elevated | False |

## Toolchain

| Tool | Version | On PATH | Path |
|---|---|---|---|
| git | git version 2.55.0.windows.5 | **no** | C:\Program Files\Git\cmd\git.exe |
| git_lfs | git-lfs/3.7.1 (GitHub; windows amd64; go 1.25.1; git b84b3384) | **no** | C:\Program Files\Git\mingw64\bin\git-lfs.exe |
| gh | not installed | - | - |
| python | Python 3.12.10 | **no** | C:\Users\Cbhea\AppData\Local\Programs\Python\Python312\python.exe |
| dotnet | runtimes only (no SDK): 8.0.21, 8.0.26, 10.0.11 | yes | C:\Program Files\dotnet\dotnet.exe |
| node | not installed | - | - |
| cmake | not installed | - | - |
| clang_cl | not installed | - | - |
| blender | not installed | - | - |
| claude | not installed | - | - |
| vscode | 1.138.0 | **no** | C:\Users\Cbhea\AppData\Local\Programs\Microsoft VS Code\bin\code.cmd |
| 7zip | not installed | - | - |

| .NET SDKs | - |
|---|---|
| Visual Studio | not installed |
| Windows SDKs | - |
| Epic Games Launcher | not installed |
| Unreal Engine | none installed |
| env UE_ROOT | - |
| env UE-LocalDataCachePath | - |
| env UE_LOCAL_DDC | - |
| env EDITOR_LOCK_DIR | - |
| git core.longpaths | - |
| git core.autocrlf | true |
| git filter.lfs.process | git-lfs filter-process |
| git user.name | cbhead |
| git lfs.fetchexclude (this clone) | - |

## This clone and CI

| | |
|---|---|
| Clone | C:\Users\Cbhea\deep-field-3d (28 chars) |
| Branch / HEAD at inventory | unreal/main / 6365ae1 2026-09-24 Merge branch 'unreal/main' of https://github.com/cbhead/deep-field-3d into unreal/main |
| Worktrees | 1 |
| Sibling DDC folder | False |
| Actions runner | none |

