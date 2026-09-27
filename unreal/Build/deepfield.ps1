<#
.SYNOPSIS
  Deep Field 3D (Unreal) on Windows: one script that makes a machine ready, then builds, tests and runs.

.DESCRIPTION
  Run it from unreal\deepfield.cmd (double-click, or `deepfield <command>` in a terminal), or on a
  machine with nothing on it yet, from the one-line bootstrap in unreal\README.md.

  Commands:
    setup    (default) Check every prerequisite and install what is missing, clone the repository
             if this copy of the script is not inside one, then build the editor.
    doctor   Check every prerequisite and report. Installs nothing, changes nothing.
    build    Build the editor target (DeepFieldEditor Win64 Development).
    test     Build, then run the automated tests headless. Optional filter: `test DF.Unit.Tower`.
    pr-check What to run before opening a PR (Build/pr-check.sh on Windows): the repository checks
             plus your workstream's ownership check, the build, then the landing gate. The workstream
             comes from the branch name (ws/NN-slug/topic) or -Ws NN. A filter (`pr-check DF.Unit`)
             runs only that and is reported PARTIAL, never OK.
    smoke    Build, then the network smoke (Build/smoke-listen.sh on Windows): a headless listen host
             and -Clients N headless clients (default 1, at most 3) that must all be admitted and seated.
    ci-local The INT pre-merge set and the nightly lane in one command (Build/ci-local.sh on Windows):
             layering, ownership (-Ws, default INT), schemas, test coverage, plan-status (a warning
             unless -Strict), build, the landing gate, the smoke. -Skip smoke,plan-status skips steps.
             Stops at the first failure and prints a summary table either way.
    int-merge  INT: land one workstream branch on main (Build/int-merge.sh on Windows, PROGRAMME.md 6.8):
             `int-merge ws/04-towers/rig -Ws 04`. Rebases the branch onto origin/main in a verify
             worktree beside the clone (int-verify; -VerifyDir moves it), then the checks, the build,
             the landing gate and the smoke, then pushes the branch and main. -NoSmoke, -DryRun (verify,
             push nothing), -Resume (continue the landing the worktree holds, e.g. after resolving a
             conflict there by hand). A conflict outside the ledger stops for a human.
    check    The fast repository checks (Python, no engine): layering, content schemas, test coverage.
    editor   Build, then open the Unreal editor on the project. -Mcp also starts UE's experimental
             Unreal MCP server at http://localhost:8000/mcp (-McpPort to move it) for an AI agent.
    play     Build, then run the game in a window. `-Map /Game/DF/Maps/Testlane/L_Testlane` for another map.
    host     Like play, but as a listen host other players can join (port 7777, or -Port).
    join     Join a host: `join 192.168.1.20` (and -Port if the host changed it).
    solution Generate the Visual Studio solution (DeepField.sln) for working on the C++.

  setup works in two passes. Pass 1 only checks, finding what is already installed wherever it is
  (off-PATH Git and Python, any Visual Studio, the engine via the launcher's records or a source
  build, an existing clone anywhere on the machine), prints what is in place and what is missing,
  and asks before changing anything. Pass 2 installs or fixes only the missing items: with winget,
  except the engine, which only Epic's launcher can install (the script opens it and waits).
  What it looks for:
    Windows 10 19041+ / 11, 64-bit       long paths enabled (one UAC prompt)
    Git for Windows + Git LFS            Python 3.9+
    Visual Studio 2022 with an MSVC toolset the engine's Windows_SDK.json accepts, a Windows SDK
    Epic Games Launcher + Unreal Engine 5.8 (the version DeepField.uproject names)
    the repository cloned with LFS, CRLF conversion off, and the art files the Mac skips

  Every step is safe to re-run: it checks first and only acts on what is missing.
  Logs: unreal\DeepField\Saved\Logs\ (build, tests) and %LOCALAPPDATA%\DeepField\ (setup).

.EXAMPLE
  deepfield setup
.EXAMPLE
  deepfield test DF.Unit
.EXAMPLE
  deepfield pr-check -Ws 04
.EXAMPLE
  deepfield ci-local -Skip smoke
.EXAMPLE
  deepfield play -Map /Game/DF/Maps/Testlane/L_Testlane
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [ValidateSet('setup', 'doctor', 'build', 'test', 'pr-check', 'smoke', 'ci-local', 'int-merge', 'check', 'editor', 'play', 'host', 'join', 'solution', 'help')]
  [string]$Command = 'setup',
  # test, pr-check, ci-local: the automation filter (default: the landing gate from test.sh). join: the host address.
  # int-merge: the branch to land.
  [Parameter(Position = 1)]
  [string]$Arg = '',
  # Where to clone when the script is not inside a clone. Default D:\DF\deepfield-3d, or C:\DF\deepfield-3d without a D: drive.
  [string]$Dir = '',
  # The engine folder (the one containing Engine\). Default: found through the launcher's records.
  [string]$EngineDir = '',
  # The map smoke, ci-local and join tests use; play and host default to the first playable (Testlane) instead.
  [string]$Map = '/Game/DF/Dev/L_Dev_Empty',
  # setup: the branch to clone (default main, the trunk), e.g. a PR branch to try before it merges.
  [string]$Branch = '',
  # pr-check: the workstream whose ownership globs apply (default: from the branch name ws/NN-slug/topic).
  # ci-local, int-merge: the ownership view (default INT, which lists everything and fails only on binaries outside every glob).
  [string]$Ws = '',
  # int-merge: skip the smoke; verify without pushing; continue the landing the verify worktree holds.
  [switch]$NoSmoke,
  [switch]$DryRun,
  [switch]$Resume,
  # int-merge: the verify worktree (default: int-verify beside the clone, or DF_INT_VERIFY_WT).
  [string]$VerifyDir = '',
  # pr-check, ci-local: the ref the ownership check diffs against.
  [string]$Base = 'origin/main',
  # smoke, ci-local: how many headless clients join the listen host (1-3: the host takes one of the 4 seats).
  [int]$Clients = 1,
  # ci-local: steps to skip, e.g. -Skip smoke or -Skip smoke,plan-status.
  [string[]]$Skip = @(),
  # ci-local: a stale STATUS.md fails the run instead of warning.
  [switch]$Strict,
  [int]$Port = 7777,
  # setup: keep the light clone (skip Megascans and the art/lighting sublevels, as the Mac does).
  [switch]$LightClone,
  # setup: do not build at the end.
  [switch]$NoBuild,
  # test: run even if the built modules are older than the source (prints what it ignores).
  [switch]$AllowStale,
  # setup: do not ask before installing what the check found missing.
  [switch]$Yes,
  # editor: also enable UE's experimental Unreal MCP server (runbook 6.1), on this launch only.
  [switch]$Mcp,
  # editor -Mcp: the port the MCP server listens on (localhost only).
  [int]$McpPort = 8000,
  # play: the match plays itself (towers built from the economy, an orbiting camera): -DFDemo.
  [switch]$Demo,
  # Keep the window open at the end (the .cmd passes this when the script was double-clicked).
  [switch]$Pause
)

Set-StrictMode -Version 1   # uninitialised variables only: JSON from vswhere and the test report has optional fields
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is 10x slower with the progress bar on 5.1

# ---------------------------------------------------------------------------------------------------
# What the project needs. Change these here and nowhere else.
# ---------------------------------------------------------------------------------------------------
$RepoUrl       = 'https://github.com/cbhead/deep-field-3d.git'
$RepoBranch    = 'main'
$EnginePatch   = 3          # UE 5.8.3: a different patch re-saves assets on open (runbook 1.2, "Why a pinned patch")
$MinFreeGB     = 150
# editor -Mcp: the Unreal MCP server and the engine toolsets it serves (Engine\Plugins\Experimental\Toolsets).
$McpPlugins    = @('ModelContextProtocol', 'EditorToolset', 'AutomationTestToolset', 'ConfigSettingsToolset', 'SlateInspectorToolset')
$MinPython     = [version]'3.9'
$MinWinBuild   = 19041
$WinSdkWanted  = '10.0.22621.0'
$WinSdkMinimum = [version]'10.0.19041.0'
# Which MSVC toolsets are accepted is read from the installed engine (Get-MsvcRules), not written here.
$VsRequired    = @('Microsoft.VisualStudio.Component.VC.Tools.x86.x64')   # the SDK is checked on disk
$VsFreshAdd    = @(   # a new Visual Studio 2022 install: what Epic's own setup guide lists
  'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
  'Microsoft.VisualStudio.Component.Windows11SDK.22621',
  'Microsoft.Net.Component.4.6.2.TargetingPack'
)
$VsWorkloads   = @(
  'Microsoft.VisualStudio.Workload.NativeDesktop',
  'Microsoft.VisualStudio.Workload.NativeGame',
  'Microsoft.VisualStudio.Workload.ManagedDesktop'
)

# ---------------------------------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------------------------------
$script:Problems = New-Object System.Collections.ArrayList
$script:DoctorOnly = ($Command -eq 'doctor')
$script:Quiet = $false        # the second setup pass prints only what it changes
# Everything the checks fill in, declared up front (strict mode refuses reads of unset variables).
$script:Repo = $null; $script:Project = $null; $script:Engine = $null; $script:EngineAssociation = '5.8'
$script:Python = $null; $script:PythonProbes = @(); $script:MsvcRules = $null
$script:OkCount = 0
$script:LandingLock = $null; $script:IM = $null   # int-merge: the lock's open handle, and the landing's state

function Write-Step([string]$Text) { if (-not $script:Quiet) { Write-Host ''; Write-Host "== $Text" -ForegroundColor Cyan } }
function Write-Ok([string]$Text)   { $script:OkCount++; if (-not $script:Quiet) { Write-Host "   [ OK ] $Text" -ForegroundColor Green } }
function Write-Info([string]$Text) { Write-Host "          $Text" }
function Write-Fix([string]$Text)  { Write-Host "   [FIX ] $Text" -ForegroundColor Yellow }
function Write-Warn2([string]$Text){ Write-Host "   [WARN] $Text" -ForegroundColor Yellow }

# A requirement that is not met. In doctor mode it is recorded and the checks go on; otherwise the
# script stops here with the fix spelled out.
function Fail([string]$What, [string]$Remedy) {
  if ($script:DoctorOnly) {
    # Checking only: a missing item is a finding, not an error.
    Write-Host "   [MISS] $What" -ForegroundColor Yellow
    if ($Remedy) { foreach ($line in $Remedy -split "`n") { Write-Host "          $line" -ForegroundColor Yellow } }
    [void]$script:Problems.Add($What)
    return
  }
  Write-Host "   [FAIL] $What" -ForegroundColor Red
  if ($Remedy) { foreach ($line in $Remedy -split "`n") { Write-Host "          $line" -ForegroundColor Red } }
  [void]$script:Problems.Add($What)
  if (-not $script:DoctorOnly) { throw [System.OperationCanceledException]::new($What) }
}

function Invoke-Native {
  # Runs an executable, returns its exit code; output goes to the console. Never throws on a non-zero exit.
  param([string]$Exe, [string[]]$Arguments)
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try { & $Exe @Arguments | Out-Host; return $LASTEXITCODE } finally { $ErrorActionPreference = $old }
}

function Get-NativeOutput {
  # Runs an executable and returns its stdout as one string, or $null if it could not run.
  param([string]$Exe, [string[]]$Arguments)
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try {
    $out = & $Exe @Arguments 2>$null
    if ($LASTEXITCODE -ne 0) { return $null }
    return (($out | Out-String).Trim())
  } catch { return $null } finally { $ErrorActionPreference = $old }
}

function Update-SessionPath {
  # winget installs update the registry PATH, not this process's; pick the new entries up.
  $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
  $user = [Environment]::GetEnvironmentVariable('Path', 'User')
  $env:Path = (@($machine, $user) | Where-Object { $_ }) -join ';'
}

function Test-Admin {
  $id = [Security.Principal.WindowsIdentity]::GetCurrent()
  return (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-Elevated([string]$PowerShellCommand) {
  # One UAC prompt for one command; waits for it. Returns the exit code.
  if (Test-Admin) {
    powershell -NoProfile -ExecutionPolicy Bypass -Command $PowerShellCommand
    return $LASTEXITCODE
  }
  $p = Start-Process powershell -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
    -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $PowerShellCommand)
  return $p.ExitCode
}

# ---------------------------------------------------------------------------------------------------
# winget
# ---------------------------------------------------------------------------------------------------
function Get-Winget {
  $w = Get-Command winget -ErrorAction SilentlyContinue
  if ($w) { return $w.Source }
  # A new Windows install ships App Installer unregistered for the user until the Store updates it.
  try {
    Add-AppxPackage -RegisterByFamilyName -MainPackage Microsoft.DesktopAppInstaller_8wekyb3d8bbwe -ErrorAction Stop
    Update-SessionPath
  } catch { }
  $w = Get-Command winget -ErrorAction SilentlyContinue
  if ($w) { return $w.Source }
  $local = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\winget.exe'
  if (Test-Path $local) { return $local }
  return $null
}

function Install-WithWinget([string]$Id, [string]$Name, [string]$Override = '') {
  if ($script:DoctorOnly) { return $false }
  $winget = Get-Winget
  if (-not $winget) {
    Fail "winget is not available, so $Name cannot be installed automatically" `
      "Open the Microsoft Store, search for 'App Installer', install or update it, then run this again.`nOr install $Name yourself and run this again."
    return $false
  }
  Write-Fix "installing $Name (winget $Id)... a UAC prompt may appear"
  $wargs = @('install', '--id', $Id, '-e', '--source', 'winget', '--accept-source-agreements', '--accept-package-agreements', '--silent')
  if ($Override) { $wargs += @('--override', $Override) }
  $rc = Invoke-Native $winget $wargs
  Update-SessionPath
  # winget's exit codes are not a reliable verdict (an already-installed package is non-zero);
  # the caller re-detects the tool and that is the verdict.
  if ($rc -ne 0) { Write-Info "winget exit code $rc; checking whether $Name is there now" }
  return $true
}

# ---------------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------------
function Assert-Windows {
  Write-Step 'Windows'
  $v = [Environment]::OSVersion.Version
  if (-not [Environment]::Is64BitOperatingSystem) { Fail 'Windows is 32-bit' 'Unreal Engine 5 needs 64-bit Windows 10 or 11.' ; return }
  if ($v.Build -lt $MinWinBuild) { Fail "Windows build $($v.Build) is older than $MinWinBuild" 'Run Windows Update (Unreal Engine 5.8 needs Windows 10 2004 or later, or Windows 11).'; return }
  Write-Ok "Windows $($v.Major).$($v.Minor) build $($v.Build), 64-bit"

  $key = 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'
  $long = (Get-ItemProperty -Path $key -Name LongPathsEnabled -ErrorAction SilentlyContinue)
  if ($long -and $long.LongPathsEnabled -eq 1) { Write-Ok 'long paths enabled' ; return }
  if ($script:DoctorOnly) { Fail 'long paths are not enabled' 'deepfield setup enables them (one UAC prompt).'; return }
  Write-Fix 'enabling long paths (Unreal and MSVC fail on paths over 260 characters): approve the UAC prompt'
  $rc = Invoke-Elevated "Set-ItemProperty -Path '$key' -Name LongPathsEnabled -Value 1 -Type DWord"
  $long = (Get-ItemProperty -Path $key -Name LongPathsEnabled -ErrorAction SilentlyContinue)
  if ($long -and $long.LongPathsEnabled -eq 1) { Write-Ok 'long paths enabled' }
  else { Fail "could not enable long paths (exit $rc)" "Settings > System > For developers > enable 'Enable long paths', then run this again." }
}

function Find-GitOffPath {
  # Git installed but not on PATH (a per-user install, GitHub Desktop's bundled git): use it rather
  # than installing a second one.
  $dirs = @(
    (Join-Path $env:ProgramFiles 'Git\cmd'),
    (Join-Path ${env:ProgramFiles(x86)} 'Git\cmd'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Git\cmd'))
  $desktop = Join-Path $env:LOCALAPPDATA 'GitHubDesktop'
  if (Test-Path $desktop) {
    $dirs += @(Get-ChildItem $desktop -Directory -Filter 'app-*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'resources\app\git\cmd' })
  }
  foreach ($d in $dirs) { if ($d -and (Test-Path (Join-Path $d 'git.exe'))) { return $d } }
  return $null
}

function Assert-Git {
  Write-Step 'Git and Git LFS'
  $git = Get-Command git -ErrorAction SilentlyContinue
  if (-not $git) {
    $off = Find-GitOffPath
    if ($off) {
      $env:Path = "$off;$env:Path"
      $git = Get-Command git -ErrorAction SilentlyContinue
      if ($git) { Write-Info "Git is installed at $off but not on PATH; using that one" }
    }
  }
  if (-not $git) {
    if ($script:DoctorOnly) { Fail 'Git is not installed' 'deepfield setup installs it (winget Git.Git).'; return }
    Install-WithWinget 'Git.Git' 'Git for Windows' | Out-Null
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
      $candidate = Join-Path $env:ProgramFiles 'Git\cmd'
      if (Test-Path (Join-Path $candidate 'git.exe')) { $env:Path = "$candidate;$env:Path"; $git = Get-Command git -ErrorAction SilentlyContinue }
    }
    if (-not $git) { Fail 'Git did not install' 'Install Git for Windows from https://git-scm.com/download/win and run this again.'; return }
  }
  Write-Ok (Get-NativeOutput 'git' @('--version'))

  $lfs = Get-NativeOutput 'git' @('lfs', 'version')
  if (-not $lfs) {
    if ($script:DoctorOnly) { Fail 'Git LFS is not installed' 'deepfield setup installs it (winget GitHub.GitLFS).'; return }
    Install-WithWinget 'GitHub.GitLFS' 'Git LFS' | Out-Null
    $lfs = Get-NativeOutput 'git' @('lfs', 'version')
    if (-not $lfs) { Fail 'Git LFS did not install' 'Install it from https://git-lfs.com and run this again.'; return }
  }
  Write-Ok $lfs
  if (-not $script:DoctorOnly) { Invoke-Native 'git' @('lfs', 'install', '--skip-repo') | Out-Null }
}

function Find-Python {
  # The Microsoft Store puts a python.exe stub on PATH that opens the Store instead of running; the
  # version probe rejects it. The py launcher comes first because python.org installs it by default.
  $candidates = @(@('py', '-3'), @('python'), @('python3'))
  # Installed but not on PATH (python.org's per-user default leaves PATH alone).
  foreach ($pattern in @((Join-Path $env:LOCALAPPDATA 'Programs\Python\Python3*\python.exe'),
      (Join-Path $env:ProgramFiles 'Python3*\python.exe'), (Join-Path $env:SystemDrive 'Python3*\python.exe'))) {
    foreach ($f in @(Get-ChildItem $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)) { $candidates += ,@($f.FullName) }
  }
  $script:PythonProbes = @()
  foreach ($candidate in $candidates) {
    $exe = $candidate[0]; $pre = @($candidate | Select-Object -Skip 1)
    $cmd = Get-Command $exe -ErrorAction SilentlyContinue
    if (-not $cmd) { continue }
    # `--version`, not `-c "..."`: Windows PowerShell 5.1 strips double quotes inside arguments it
    # passes to native programs, which turned the old probe into a SyntaxError on every machine.
    $out = Get-NativeOutput $exe ($pre + @('--version'))
    $where = $cmd.Source; if (-not $where) { $where = $exe }
    if ($out -and $out -match 'Python (\d+\.\d+\.\d+)') {
      $v = $Matches[1]
      $script:PythonProbes += "$where -> Python $v"
      if ([version]$v -ge $MinPython) { return [pscustomobject]@{ Exe = $exe; Pre = $pre; Version = $v } }
    } else {
      $hint = ''; if ($where -like '*WindowsApps*') { $hint = ' (the Microsoft Store placeholder)' }
      $script:PythonProbes += "$where -> did not run$hint"
    }
  }
  return $null
}

function Assert-Python {
  Write-Step 'Python'
  $script:Python = Find-Python
  if (-not $script:Python) {
    if ($script:DoctorOnly) { Fail "Python $MinPython+ is not installed" 'deepfield setup installs it (winget Python.Python.3.12).'; return }
    Install-WithWinget 'Python.Python.3.12' 'Python 3.12' '/quiet InstallAllUsers=0 PrependPath=1 Include_launcher=1 Include_test=0' | Out-Null
    $script:Python = Find-Python
    if (-not $script:Python) {
      if ($script:PythonProbes.Count -gt 0) { Write-Info 'what was found:'; foreach ($pp in $script:PythonProbes) { Write-Info "  $pp" } }
      else { Write-Info 'no python.exe or py.exe was found on PATH or in the usual install folders' }
      Fail 'Python did not install' "Install Python 3.12 from https://www.python.org/downloads/ (tick 'Add python.exe to PATH'),`nor turn off the Store's python.exe alias in Settings > Apps > Advanced app settings > App execution aliases, then run this again."
      return
    }
  }
  Write-Ok "Python $($script:Python.Version) ($($script:Python.Exe))"
}

function Invoke-Python([string[]]$Arguments) {
  return (Invoke-Native $script:Python.Exe ($script:Python.Pre + $Arguments))
}

# --- Visual Studio -----------------------------------------------------------------------------------
function Get-VsWhere {
  $p = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path $p) { return $p }
  return $null
}

function Get-VsInstances {
  $vswhere = Get-VsWhere
  if (-not $vswhere) { return @() }
  $json = Get-NativeOutput $vswhere @('-products', '*', '-prerelease', '-format', 'json', '-utf8')
  if (-not $json) { return @() }
  return @($json | ConvertFrom-Json)
}

function ConvertTo-MsvcVersion([string]$Text, [bool]$High) {
  # "14.44.35211" -> 14.44.35211; "14.44" -> 14.44.0 (low end) or 14.44.99999 (high end).
  $parts = @($Text.Trim() -split '\.')
  if ($parts.Count -lt 2) { return $null }
  $build = 0; if ($High) { $build = 99999 }
  if ($parts.Count -ge 3) { $build = [int]$parts[2] }
  try { return [version]("{0}.{1}.{2}" -f [int]$parts[0], [int]$parts[1], $build) } catch { return $null }
}

function ConvertTo-MsvcRanges($Values) {
  $out = @()
  foreach ($t in @($Values)) {
    if ($t -isnot [string]) { continue }
    $ends = @($t -split '-')
    $lo = ConvertTo-MsvcVersion $ends[0] $false
    $hi = ConvertTo-MsvcVersion $ends[-1] $true
    if ($lo -and $hi) { $out += [pscustomobject]@{ Lo = $lo; Hi = $hi; Text = $t } }
  }
  return $out
}

function Get-MsvcRules {
  # The engine's own compiler rules (Engine\Config\Windows\Windows_SDK.json) once the engine is
  # installed; UBT applies exactly these. Before that, only the ranges known to be refused, so a
  # machine is never sent to reinstall Visual Studio over a guess.
  $root = $null; if ($script:Engine) { $root = $script:Engine.Root }
  if ($script:MsvcRules -and $script:MsvcRules.Root -eq $root) { return $script:MsvcRules }
  $rules = [pscustomobject]@{
    Root = $root; Source = 'built-in minimum (the exact rules are read from the engine once it is installed)'
    Min = [version]'14.38.33130'; Banned = @(ConvertTo-MsvcRanges @('14.39-14.43')); Preferred = @()
  }
  if ($root) {
    $file = Join-Path $root 'Engine\Config\Windows\Windows_SDK.json'
    if (Test-Path $file) {
      try {
        $text = (Get-Content $file -Raw) -replace '(?m)^\s*//.*$', ''
        $j = $text | ConvertFrom-Json
        $banned = @(); $preferred = @(); $min = $null
        foreach ($prop in $j.PSObject.Properties) {
          $n = $prop.Name
          if ($n -match 'Clang|Intel|Sdk|Windows') { continue }
          if ($n -notmatch 'VisualCpp|Msvc|VCTools|Toolchain|Compiler') { continue }
          if ($n -match 'Banned') { $banned += @(ConvertTo-MsvcRanges $prop.Value) }
          elseif ($n -match 'Preferred') { $preferred += @(ConvertTo-MsvcRanges $prop.Value) }
          elseif ($n -match 'Minimum' -and $prop.Value -is [string]) { $min = ConvertTo-MsvcVersion $prop.Value $false }
        }
        if ($min -or $banned.Count -or $preferred.Count) {
          $rules = [pscustomobject]@{ Root = $root; Source = $file; Min = $min; Banned = $banned; Preferred = $preferred }
        }
      } catch { Write-Verbose "could not read ${file}: $($_.Exception.Message)" }
    }
  }
  $script:MsvcRules = $rules
  return $rules
}

function Test-MsvcAccepted([version]$v, [version]$Family = $null) {
  # 'preferred', 'allowed' (UBT warns and builds) or 'banned' (UBT refuses), per Get-MsvcRules.
  # $v is the compiler's own build (from cl.exe) and $Family the toolset folder's name. They differ once
  # Visual Studio services the compiler in place: VS 2022 17.14.x keeps VC\Tools\MSVC\14.44.35207 while
  # cl.exe moves on. The minimum and the refused ranges are about compiler builds (Windows_SDK.json:
  # "14.44.35207 ... Resolved with 14.44.35211"); the preferred ranges name the family ("Version number
  # is the MSVC family, which is the version in the Visual Studio folder").
  if (-not $Family) { $Family = $v }
  $rules = Get-MsvcRules
  if ($rules.Min -and $v -lt $rules.Min) { return 'banned' }
  foreach ($r in $rules.Banned) { if ($v -ge $r.Lo -and $v -le $r.Hi) { return 'banned' } }
  foreach ($r in $rules.Preferred) { if ($Family -ge $r.Lo -and $Family -le $r.Hi) { return 'preferred' } }
  return 'allowed'
}

function ConvertTo-ClVersion($VersionInfo, [version]$Family) {
  # The compiler build from cl.exe's version resource. Its product version is the toolset's (14.x); its
  # file version is the compiler's (19.x) with the same minor and build. Falls back to the folder name.
  if ($VersionInfo) {
    if ($VersionInfo.ProductMajorPart -eq 14) { return [version]("14.{0}.{1}" -f $VersionInfo.ProductMinorPart, $VersionInfo.ProductBuildPart) }
    if ($VersionInfo.FileMajorPart -eq 19) { return [version]("14.{0}.{1}" -f $VersionInfo.FileMinorPart, $VersionInfo.FileBuildPart) }
  }
  return $Family
}

function Get-VsToolsets([string]$InstallPath) {
  $root = Join-Path $InstallPath 'VC\Tools\MSVC'
  if (-not (Test-Path $root)) { return @() }
  $out = @()
  foreach ($d in Get-ChildItem $root -Directory) {
    $family = $null
    $cl = Join-Path $d.FullName 'bin\Hostx64\x64\cl.exe'
    if ([version]::TryParse($d.Name, [ref]$family) -and (Test-Path $cl)) {
      $vi = $null; try { $vi = (Get-Item $cl).VersionInfo } catch { $vi = $null }
      $v = ConvertTo-ClVersion $vi $family
      $out += [pscustomobject]@{ Version = $v; Family = $family; Verdict = (Test-MsvcAccepted $v $family) }
    }
  }
  return $out
}

function Format-Toolset($T) {
  # "14.44.35207" when the folder and the compiler agree, "14.44.35207 (compiler 14.44.35217)" when not.
  if ($T.Family -and $T.Version -ne $T.Family) { return "$($T.Family) (compiler $($T.Version))" }
  return "$($T.Version)"
}

function Get-WindowsSdks {
  $inc = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
  if (-not (Test-Path $inc)) { return @() }
  $out = @()
  foreach ($d in Get-ChildItem $inc -Directory) {
    $v = $null
    if ([version]::TryParse($d.Name, [ref]$v) -and (Test-Path (Join-Path $d.FullName 'um\windows.h'))) { $out += $v }
  }
  return $out
}

function Format-Toolsets($Vs) {
  $t = @($Vs.Toolsets | Sort-Object Version -Descending | ForEach-Object { "$(Format-Toolset $_) $($_.Verdict)" })
  if ($t.Count -eq 0) { return 'none (no VC\Tools\MSVC\<version>\bin\Hostx64\x64\cl.exe)' }
  return ($t -join ', ')
}

function Get-VsVerdict {
  # The best Visual Studio for UBT: a 2022/2026 install with the components and an accepted toolset.
  $best = $null
  foreach ($vs in Get-VsInstances) {
    $major = 0
    try { $major = ([version]$vs.installationVersion).Major } catch { }
    if ($major -lt 17) { continue }
    $toolsets = @(Get-VsToolsets $vs.installationPath)
    $good = @($toolsets | Where-Object { $_.Verdict -ne 'banned' } | Sort-Object Version -Descending)
    $cand = [pscustomobject]@{
      Instance = $vs; Major = $major; Toolsets = $toolsets; Good = $good
      Name = "$($vs.displayName) $($vs.catalog.productDisplayVersion)"
    }
    if (-not $best -or ($good.Count -gt 0 -and $best.Good.Count -eq 0)) { $best = $cand }
  }
  return $best
}

function Assert-VisualStudio {
  Write-Step 'Visual Studio and the C++ toolset'
  $vs = Get-VsVerdict
  $freshArgs = @()
  foreach ($c in $VsWorkloads + $VsFreshAdd) { $freshArgs += @('--add', $c) }

  if (-not $vs) {
    if ($script:DoctorOnly) { Fail 'Visual Studio 2022 is not installed' 'deepfield setup installs Visual Studio 2022 Community with the C++ game workloads (about 20 GB).'; return }
    Write-Info 'Visual Studio 2022 Community with the C++ game development workloads: about 20 GB, 15-40 minutes.'
    Install-WithWinget 'Microsoft.VisualStudio.2022.Community' 'Visual Studio 2022 Community' `
      ((@('--wait', '--passive', '--norestart', '--includeRecommended') + $freshArgs) -join ' ') | Out-Null
    $vs = Get-VsVerdict
    if (-not $vs) { Fail 'Visual Studio did not install' 'Install Visual Studio 2022 Community from https://visualstudio.microsoft.com/ with the workloads "Desktop development with C++" and "Game development with C++", then run this again.'; return }
  }

  # Components: vswhere -requires lists the instances that have one, and we look for ours among them.
  # (Not -path: vswhere rejects it combined with any other selection option, error 0x7f and no output,
  # which read as every component missing.)
  $vswhere = Get-VsWhere
  $missing = @()
  foreach ($c in $VsRequired) {
    $ids = @("$(Get-NativeOutput $vswhere @('-products', '*', '-prerelease', '-requires', $c, '-property', 'instanceId'))" -split '\r?\n' |
      ForEach-Object { $_.Trim() })
    if ($ids -notcontains $vs.Instance.instanceId) { $missing += $c }
  }
  $needUpdate = ($vs.Good.Count -eq 0)
  if ($missing.Count -gt 0 -or $needUpdate) {
    $why = @()
    if ($missing.Count -gt 0) { $why += "missing components: $($missing -join ', ')" }
    if ($needUpdate) { $why += "no MSVC toolset the engine accepts (found: $(Format-Toolsets $vs))" }
    if ($script:DoctorOnly) { Fail "$($vs.Name): $($why -join '; ')" 'deepfield setup updates and modifies it.'; return }
    $installer = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
    $path = $vs.Instance.installationPath
    $updateRc = 'not run'
    if ($needUpdate) {
      Write-Fix "updating $($vs.Name) to the latest release (brings the current MSVC toolset); approve the UAC prompt"
      $p = Start-Process $installer -Wait -PassThru -ArgumentList @('update', '--installPath', "`"$path`"", '--passive', '--norestart')
      $updateRc = $p.ExitCode
      Write-Info "Visual Studio Installer exit code $($p.ExitCode)"
    }
    # Only ids every supported Visual Studio knows: the workloads and what was found missing.
    $addArgs = @()
    foreach ($c in $VsWorkloads + $missing) { $addArgs += @('--add', $c) }
    if (@(Get-WindowsSdks | Where-Object { $_ -ge $WinSdkMinimum }).Count -eq 0 -and $vs.Major -eq 17) { $addArgs += @('--add', 'Microsoft.VisualStudio.Component.Windows11SDK.22621') }
    Write-Fix "adding the C++ game workloads to $($vs.Name); approve the UAC prompt"
    $p = Start-Process $installer -Wait -PassThru -ArgumentList (@('modify', '--installPath', "`"$path`"", '--passive', '--norestart', '--includeRecommended') + $addArgs)
    $modifyRc = $p.ExitCode
    Write-Info "Visual Studio Installer exit code $($p.ExitCode) (3010 means a restart is needed later)"
    $vs = Get-VsVerdict
    if (-not $vs -or $vs.Good.Count -eq 0) {
      $rules = Get-MsvcRules
      Write-Info "Visual Studio found: $(if ($vs) { "$($vs.Name) at $($vs.Instance.installationPath)" } else { 'none' })"
      Write-Info "MSVC toolsets in it: $(if ($vs) { Format-Toolsets $vs } else { '-' })"
      Write-Info "rules applied: $($rules.Source)"
      if ($rules.Min) { Write-Info "  minimum $($rules.Min)" }
      foreach ($r in $rules.Banned) { Write-Info "  refused $($r.Text)" }
      foreach ($r in $rules.Preferred) { Write-Info "  preferred $($r.Text)" }
      # Everything needed to diagnose this goes into the failure itself: the red block is what people
      # copy, and the info lines above it are easy to miss.
      $foundText = 'no Visual Studio 2022/2026 instance'
      if ($vs) { $foundText = "$($vs.Name) at $($vs.Instance.installationPath); MSVC toolsets: $(Format-Toolsets $vs)" }
      $ruleText = @()
      if ($rules.Min) { $ruleText += "minimum $($rules.Min)" }
      foreach ($r in $rules.Banned) { $ruleText += "refused $($r.Text)" }
      foreach ($r in $rules.Preferred) { $ruleText += "preferred $($r.Text)" }
      if ($ruleText.Count -eq 0) { $ruleText = @('none read') }
      Fail 'Visual Studio still has no MSVC toolset the engine accepts' ("Found: $foundText`n" +
        "Engine rules ($($rules.Source)): $($ruleText -join '; ')`n" +
        "Visual Studio Installer exit codes: update $updateRc, modify $modifyRc (0 = done, 3010 = done but needs a restart; anything else = it did not finish)`n" +
        "Fix: Visual Studio Installer > Update, then Modify > Individual components: tick an 'MSVC ... x64/x86 build tools' entry whose version`n" +
        "is preferred above (or at least not refused), and 'Windows 11 SDK (10.0.22621.0)'. Then run this again. Paste this whole block when asking for help.")
      return
    }
  }
  Write-Ok "$($vs.Name) at $($vs.Instance.installationPath)"
  $top = $vs.Good[0]
  Write-Ok "MSVC $(Format-Toolset $top) ($($top.Verdict); rules: $(if ($script:Engine) { 'the engine''s Windows_SDK.json' } else { 'built-in minimum until the engine is installed' }))"
  $banned = @($vs.Toolsets | Where-Object { $_.Verdict -eq 'banned' })
  if ($banned.Count -gt 0) { Write-Info "also present, ignored by UBT: $((@($banned | ForEach-Object { Format-Toolset $_ }) -join ', '))" }

  $sdks = @(Get-WindowsSdks | Sort-Object -Descending)
  $ok = @($sdks | Where-Object { $_ -ge $WinSdkMinimum })
  if ($ok.Count -eq 0) {
    Fail "no Windows 10/11 SDK $WinSdkMinimum or later" "Visual Studio Installer > Modify > Individual components > 'Windows 11 SDK ($WinSdkWanted)', then run this again."
    return
  }
  if (@($ok | Where-Object { $_.ToString() -eq $WinSdkWanted }).Count -gt 0) { Write-Ok "Windows SDK $WinSdkWanted" }
  else { Write-Ok "Windows SDK $($ok[0]) (the engine prefers $WinSdkWanted; this one works)" }
}

# --- Unreal Engine -----------------------------------------------------------------------------------
function Get-EngineVersion([string]$Root) {
  $bv = Join-Path $Root 'Engine\Build\Build.version'
  if (-not (Test-Path $bv)) { return $null }
  try {
    $j = Get-Content $bv -Raw | ConvertFrom-Json
    return [version]("{0}.{1}.{2}" -f $j.MajorVersion, $j.MinorVersion, $j.PatchVersion)
  } catch { return $null }
}

function Find-Engine([string]$Association) {
  $candidates = New-Object System.Collections.ArrayList
  if ($EngineDir) { [void]$candidates.Add($EngineDir) }
  if ($env:UE_ROOT) { [void]$candidates.Add($env:UE_ROOT) }
  # The launcher's own record of what it installed.
  $dat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
  if (Test-Path $dat) {
    try {
      foreach ($i in (Get-Content $dat -Raw | ConvertFrom-Json).InstallationList) {
        if ($i.AppName -eq "UE_$Association") { [void]$candidates.Add($i.InstallLocation) }
      }
    } catch { }
  }
  foreach ($hive in @('HKLM:\SOFTWARE\EpicGames\Unreal Engine', 'HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine')) {
    $k = Get-ItemProperty -Path "$hive\$Association" -Name InstalledDirectory -ErrorAction SilentlyContinue
    if ($k) { [void]$candidates.Add($k.InstalledDirectory) }
  }
  # Engines built from source register themselves here (value name = a GUID, data = the folder).
  $builds = Get-Item 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
  if ($builds) { foreach ($n in $builds.GetValueNames()) { [void]$candidates.Add([string]$builds.GetValue($n)) } }
  [void]$candidates.Add((Join-Path $env:ProgramFiles "Epic Games\UE_$Association"))
  foreach ($drive in Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue) {
    [void]$candidates.Add((Join-Path $drive.Root "Epic Games\UE_$Association"))
    [void]$candidates.Add((Join-Path $drive.Root "Program Files\Epic Games\UE_$Association"))
    [void]$candidates.Add((Join-Path $drive.Root "UE_$Association"))
    [void]$candidates.Add((Join-Path $drive.Root "Unreal\UE_$Association"))
  }
  foreach ($c in $candidates) {
    if (-not $c) { continue }
    $v = Get-EngineVersion $c
    if ($v -and "$($v.Major).$($v.Minor)" -eq $Association -and (Test-Path (Join-Path $c 'Engine\Build\BatchFiles\Build.bat'))) {
      return [pscustomobject]@{ Root = (Resolve-Path $c).Path; Version = $v }
    }
  }
  return $null
}

function Get-LauncherExe {
  foreach ($p in @(
      (Join-Path ${env:ProgramFiles(x86)} 'Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe'),
      (Join-Path ${env:ProgramFiles(x86)} 'Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe'),
      (Join-Path $env:ProgramFiles 'Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe'))) {
    if (Test-Path $p) { return $p }
  }
  return $null
}

function Assert-Engine {
  Write-Step 'Unreal Engine'
  $assoc = $script:EngineAssociation
  $engine = Find-Engine $assoc
  if (-not $engine) {
    if ($script:DoctorOnly) { Fail "Unreal Engine $assoc is not installed" 'deepfield setup installs the Epic Games Launcher and walks you through installing the engine.'; return }
    $launcher = Get-LauncherExe
    if (-not $launcher) {
      Install-WithWinget 'EpicGames.EpicGamesLauncher' 'Epic Games Launcher' | Out-Null
      $launcher = Get-LauncherExe
      if (-not $launcher) { Fail 'the Epic Games Launcher did not install' 'Install it from https://store.epicgames.com/download and run this again.'; return }
    }
    Write-Ok 'Epic Games Launcher installed'
    Write-Host ''
    Write-Host "   Unreal Engine $assoc can only be installed from the Epic Games Launcher. Opening it now." -ForegroundColor Yellow
    Write-Host '   In the launcher:' -ForegroundColor Yellow
    Write-Host '     1. Sign in (a free Epic account is enough).' -ForegroundColor Yellow
    Write-Host "     2. Unreal Engine (left) > Library > the + next to ENGINE VERSIONS > pick $assoc.$EnginePatch > Install." -ForegroundColor Yellow
    Write-Host '        Keep the default location. In Options, tick "Editor symbols for debugging" (~60 GB): without it a' -ForegroundColor Yellow
    Write-Host '        crash callstack shows every engine frame as UnknownFunction. The rest of the defaults are right.' -ForegroundColor Yellow
    Write-Host '     3. Wait for the download to finish (about 40 GB; it can take an hour or more).' -ForegroundColor Yellow
    Start-Process $launcher | Out-Null
    while (-not $engine) {
      Write-Host ''
      $answer = Read-Host "   Press Enter once Unreal Engine $assoc shows as installed (or type Q to stop and run 'deepfield setup' again later)"
      if ($answer -match '^[qQ]') { Fail "Unreal Engine $assoc is not installed yet" 'Finish the install in the Epic Games Launcher, then run deepfield setup again. Everything done so far is kept.'; return }
      $engine = Find-Engine $assoc
      if (-not $engine) { Write-Warn2 "not found yet. If you installed it somewhere unusual, run: deepfield setup -EngineDir `"<folder containing Engine>`"" }
    }
  }
  $script:Engine = $engine
  Write-Ok "Unreal Engine $($engine.Version) at $($engine.Root)"
  if ($engine.Version.Build -ne $EnginePatch) {
    Write-Warn2 "the project is developed on $assoc.$EnginePatch; $($engine.Version) re-saves assets when they are opened."
    Write-Info "Update it in the launcher (Library > the engine tile's dropdown), and do not commit re-saved .uasset files meanwhile."
  }
  if ($env:UE_ROOT -ne $engine.Root -and -not $script:DoctorOnly) {
    [Environment]::SetEnvironmentVariable('UE_ROOT', $engine.Root, 'User')
    $env:UE_ROOT = $engine.Root
    Write-Fix "set UE_ROOT=$($engine.Root) for your user (new terminals see it)"
  }
}

# --- The repository ----------------------------------------------------------------------------------
function Find-RepoFrom([string]$Start) {
  $d = $Start
  while ($d) {
    if (Test-Path (Join-Path $d 'unreal\DeepField\DeepField.uproject')) { return (Resolve-Path $d).Path }
    $parent = Split-Path $d -Parent
    if ($parent -eq $d) { break }
    $d = $parent
  }
  return $null
}

function Test-IsClone([string]$Root) {
  return (Test-Path (Join-Path $Root 'unreal\DeepField\DeepField.uproject')) -and (Test-Path (Join-Path $Root '.git'))
}

function Find-ExistingClones {
  # Every clone of this repository the machine already has, best guess first. Only if the cheap
  # places have none does it search the drives (three folders deep, skipping system folders).
  $found = New-Object System.Collections.ArrayList
  $add = { param($d) if ($d -and (Test-IsClone $d)) { $r = (Resolve-Path $d).Path; if (-not $found.Contains($r)) { [void]$found.Add($r) } } }
  if ($Dir) { & $add $Dir }
  foreach ($start in @($PSScriptRoot, (Get-Location).Path)) { if ($start) { $r = Find-RepoFrom $start; if ($r) { & $add $r } } }
  $remembered = Join-Path $env:LOCALAPPDATA 'DeepField\repo.txt'
  if (Test-Path $remembered) { & $add ((Get-Content $remembered -TotalCount 1).Trim()) }
  $drives = @(Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue | Where-Object {
      try { ([IO.DriveInfo]::new($_.Root)).DriveType -eq 'Fixed' } catch { $false } })
  foreach ($d in $drives) { & $add (Join-Path $d.Root 'DF\deepfield-3d') }
  foreach ($parent in @('source\repos', 'Documents\GitHub', 'GitHub', 'repos', 'git', 'dev', 'code', 'src', 'projects', 'Projects', 'Desktop', 'Documents', 'Downloads')) {
    $p = Join-Path $env:USERPROFILE $parent
    if (Test-Path $p) { foreach ($c in @(Get-ChildItem $p -Directory -ErrorAction SilentlyContinue)) { & $add $c.FullName } }
  }
  if ($found.Count -gt 0) { return @($found) }

  if (-not $script:Quiet) { Write-Info 'looking for an existing clone on your drives (a few seconds)...' }
  $skip = @('Windows', 'Program Files', 'Program Files (x86)', 'ProgramData', '$Recycle.Bin', 'System Volume Information',
    'AppData', 'node_modules', 'Epic Games', 'Microsoft Visual Studio', 'Windows Kits', 'Recovery', 'PerfLogs')
  foreach ($d in $drives) {
    $level = @(Get-Item $d.Root)
    for ($depth = 1; $depth -le 3; $depth++) {
      $next = @()
      foreach ($dir in $level) {
        foreach ($c in @(Get-ChildItem $dir.FullName -Directory -ErrorAction SilentlyContinue)) {
          if ($skip -contains $c.Name -or $c.Name.StartsWith('.')) { continue }
          & $add $c.FullName
          $next += $c
        }
      }
      $level = $next
    }
  }
  return @($found)
}

function Save-RepoLocation([string]$Repo) {
  try {
    $dir = Join-Path $env:LOCALAPPDATA 'DeepField'
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Set-Content -Path (Join-Path $dir 'repo.txt') -Value $Repo
  } catch { Write-Verbose "could not remember the clone location: $($_.Exception.Message)" }
}

function Get-DefaultCloneDir {
  $d = Get-PSDrive -Name D -PSProvider FileSystem -ErrorAction SilentlyContinue
  if ($d -and (Test-Path 'D:\') -and ([IO.DriveInfo]::new('D').DriveType -eq 'Fixed')) { return 'D:\DF\deepfield-3d' }
  return (Join-Path $env:SystemDrive 'DF\deepfield-3d')
}

function Assert-Repo {
  Write-Step 'The repository'
  $repo = $null
  $clones = @(Find-ExistingClones)
  if ($clones.Count -gt 0) {
    $repo = $clones[0]
    if ($clones.Count -gt 1 -and -not $script:Quiet) {
      Write-Info "other clones on this machine (pass -Dir to use one of them instead): $((@($clones | Select-Object -Skip 1)) -join '; ')"
    }
  }
  if (-not $repo) {
    $target = $Dir; if (-not $target) { $target = Get-DefaultCloneDir }
    if (Test-Path (Join-Path $target 'unreal\DeepField\DeepField.uproject')) {
      Fail "$target has the project but no .git folder (a ZIP download?), so it cannot be updated or fetch its LFS files" 'Move or delete that folder, or pass -Dir with another location, and run this again.'
      return
    }
    if (-not $repo) {
      if ($script:DoctorOnly) { Fail "no clone of the repository on this machine" "setup clones it to $target (-Dir picks another folder)."; return }
      $parent = Split-Path $target -Parent
      $free = Get-FreeGB $parent
      if ($null -ne $free -and $free -lt $MinFreeGB) {
        Write-Warn2 ("{0:N0} GB free on {1}; the clone, DDC and build output want {2}+ GB. Pass -Dir to clone elsewhere." -f $free, $parent, $MinFreeGB)
      }
      New-Item -ItemType Directory -Force -Path $parent | Out-Null
      $cloneBranch = $RepoBranch; if ($Branch) { $cloneBranch = $Branch }
      Write-Fix "cloning $RepoUrl ($cloneBranch) into $target"
      $rc = Invoke-Native 'git' @('clone', '--branch', $cloneBranch, '-c', 'core.autocrlf=false', '-c', 'core.longpaths=true', $RepoUrl, $target)
      if ($rc -ne 0) { Fail "git clone failed (exit $rc)" 'Check the network connection, then run this again.'; return }
      $repo = (Resolve-Path $target).Path
    }
  }
  $script:Repo = $repo
  $script:Project = Join-Path $repo 'unreal\DeepField\DeepField.uproject'
  if (-not $script:DoctorOnly) { Save-RepoLocation $repo }
  Write-Ok "clone at $repo"

  $branch = Get-NativeOutput 'git' @('-C', $repo, 'rev-parse', '--abbrev-ref', 'HEAD')
  if ($branch -eq 'unreal/main') {
    # Frozen since 2026-09-26 (ADR-0029); the Unreal work continues on main. Say so once, never switch.
    if (-not $script:Quiet) {
      $move = 'switch main'
      if (-not (Get-NativeOutput 'git' @('-C', $repo, 'rev-parse', '--verify', '--quiet', 'refs/heads/main'))) { $move = 'switch -c main --track origin/main' }
      Write-Warn2 'branch unreal/main is frozen (ADR-0029): main is the trunk now, so this checkout gets no new work'
      Write-Info "Move it to main yourself (setup never switches branches): git -C `"$repo`" fetch origin; git -C `"$repo`" $move; git -C `"$repo`" merge --ff-only origin/main"
    }
  } elseif ($branch) { Write-Ok "branch $branch" }

  if (-not $script:DoctorOnly) {
    Invoke-Native 'git' @('-C', $repo, 'config', 'core.longpaths', 'true') | Out-Null
  }

  # Line endings. The JSON content is hashed in the join handshake, so a CRLF checkout disagrees
  # with every other machine about the same commit (Build/windows-bringup.md 3).
  $crlf = Get-NativeOutput 'git' @('-C', $repo, 'config', '--get', 'core.autocrlf')
  $converted = @(Get-CrlfCheckout $repo)
  if (($crlf -and $crlf -ne 'false') -or $converted.Count -gt 0) {
    $what = 'core.autocrlf is not false'
    if ($converted.Count -gt 0) { $what = "$($converted.Count) file(s) committed with LF are CRLF on disk, e.g. $($converted[0])" }
    if ($script:DoctorOnly) { Fail "the clone converts line endings ($what)" 'deepfield setup fixes it when the working tree has no uncommitted changes.'; }
    else {
      $dirty = Get-NativeOutput 'git' @('-C', $repo, 'status', '--porcelain')
      if ($dirty) {
        Write-Warn2 "the clone converts line endings ($what), and there are uncommitted changes, so it is left alone."
        Write-Info 'Commit or stash them, then run deepfield setup again.'
      } else {
        Write-Fix 'turning off line-ending conversion for this clone and re-checking the files out as committed'
        Invoke-Native 'git' @('-C', $repo, 'config', 'core.autocrlf', 'false') | Out-Null
        $converted = @(Get-CrlfCheckout $repo)
        if ($converted.Count -gt 0) { [void](Repair-CrlfCheckout $repo $converted) }
        $left = @(Get-CrlfCheckout $repo)
        if ($left.Count -gt 0) { Fail "$($left.Count) file(s) are still CRLF on disk, e.g. $($left[0])" "Run: git -C `"$repo`" rm --cached -r -q . ; git -C `"$repo`" reset --hard -q" }
        else { Write-Ok "line endings as committed ($($converted.Count) file(s) re-checked out)" }
      }
    }
  } else { Write-Ok 'line endings as committed (core.autocrlf=false, no file converted)' }

  # LFS content: the full set on Windows (the Mac's .lfsconfig skips the art sublevels).
  if (-not $script:DoctorOnly) {
    if (-not $LightClone) {
      $ex = Get-NativeOutput 'git' @('-C', $repo, 'config', '--local', '--get', 'lfs.fetchexclude')
      if ($null -eq $ex) {
        # An empty local value overrides .lfsconfig. Windows PowerShell drops empty arguments to
        # native commands, so this one goes through Start-Process's raw argument string.
        Start-Process git -NoNewWindow -Wait -ArgumentList "-C `"$repo`" config --local lfs.fetchexclude `"`"" | Out-Null
        Write-Fix 'this clone fetches every LFS file (the Mac skips Megascans and the art sublevels; -LightClone keeps that)'
      }
    }
    Write-Info 'fetching LFS files (only what is missing)...'
    $rc = Invoke-Native 'git' @('-C', $repo, 'lfs', 'pull')
    if ($rc -ne 0) { Fail "git lfs pull failed (exit $rc)" 'Check the network connection, then run this again.'; return }
  }
  $pointer = Get-ChildItem (Join-Path $repo 'unreal\DeepField\Content') -Recurse -Include *.uasset, *.umap -ErrorAction SilentlyContinue |
    Where-Object { $_.Length -lt 200 } | Select-Object -First 1
  if ($pointer -and ((Get-Content $pointer.FullName -TotalCount 1) -match 'git-lfs')) {
    if ($LightClone) { Write-Warn2 'some assets are LFS pointers (expected with -LightClone for the art sublevels)' }
    else { Fail "LFS pointer instead of a file: $($pointer.FullName)" "Run: git -C `"$repo`" lfs pull" }
  } else { Write-Ok 'LFS files present' }

  $free = Get-FreeGB $repo
  if ($null -ne $free) {
    if ($free -lt 50) { Write-Warn2 ("{0:N0} GB free on the clone's drive; the first build and the DDC need tens of GB" -f $free) }
    else { Write-Ok ("{0:N0} GB free on the clone's drive" -f $free) }
  }

  # The DDC beside the clone (DefaultEngine.ini's DDC store). The project config alone does not move
  # Unreal Zen Storage, which the editor also writes: without a per-machine local DDC path it keeps its
  # store in %LOCALAPPDATA%\UnrealEngine\Common\Zen\Data. With UE-LocalDataCachePath set, Zen follows
  # it into <DDC>\Zen, so the whole cache sits in one folder (and under the Defender exclusion, runbook 1.4).
  $ddc = Join-Path (Split-Path $repo -Parent) 'DDC'
  $ddcVar = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'User')
  if ($ddcVar -eq $ddc) { Write-Ok "UE-LocalDataCachePath=$ddc (the DDC, Zen's store inside it)" }
  elseif ($script:DoctorOnly) { Write-Warn2 "UE-LocalDataCachePath is $(if ($ddcVar) { $ddcVar } else { 'unset' }), not $ddc; Zen keeps its store in %LOCALAPPDATA%. Run: deepfield setup" }
  else {
    if (-not (Test-Path $ddc)) { New-Item -ItemType Directory $ddc | Out-Null }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $ddc, 'User')
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $ddc, 'Process')
    Write-Fix "set UE-LocalDataCachePath=$ddc for your user, so Zen keeps its store in the DDC (new terminals see it)"
  }
}

function Get-CrlfCheckout([string]$Root) {
  # Files git stores with LF that are CRLF on disk: a checkout made while core.autocrlf was true (Git for
  # Windows' installer default). Git still calls them clean, because it trusts their timestamps, so neither
  # `git status` nor `reset --hard` notices; the GPU box's clone had 633 of them, the content JSON among
  # them, which the join handshake hashes (Build/windows-bringup.md 1).
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try { $lines = @(& git -C $Root -c core.quotepath=off ls-files --eol 2>$null) } finally { $ErrorActionPreference = $old }
  return @($lines | ForEach-Object {
      $t = "$_" -split "`t", 2
      if ($t.Count -eq 2 -and $t[0] -match '^i/lf\s+w/crlf\s') { $t[1] }
    })
}

function Repair-CrlfCheckout([string]$Root, [string[]]$Paths) {
  # Delete, then check out again: git will not rewrite a file whose timestamp matches its index entry.
  $list = Join-Path $env:TEMP "deepfield-crlf-$PID.txt"
  [IO.File]::WriteAllText($list, (($Paths -join "`n") + "`n"), (New-Object Text.UTF8Encoding($false)))
  foreach ($p in $Paths) { Remove-Item -LiteralPath (Join-Path $Root $p) -Force -ErrorAction SilentlyContinue }
  $rc = Invoke-Native 'git' @('--literal-pathspecs', '-C', $Root, 'checkout', "--pathspec-from-file=$list", '--')
  Remove-Item -LiteralPath $list -ErrorAction SilentlyContinue
  return $rc
}

function Get-FreeGB([string]$Path) {
  try {
    $root = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Path))
    return [math]::Floor(([IO.DriveInfo]::new($root)).AvailableFreeSpace / 1GB)
  } catch { return $null }
}

function Read-EngineAssociation {
  # The engine version comes from the project file, so a bump there is picked up here.
  $assoc = '5.8'
  if ($script:Project -and (Test-Path $script:Project)) {
    try { $assoc = (Get-Content $script:Project -Raw | ConvertFrom-Json).EngineAssociation } catch { }
  }
  $script:EngineAssociation = $assoc
}

# ---------------------------------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------------------------------
function Get-Paths {
  $e = $script:Engine.Root
  return [pscustomobject]@{
    BuildBat  = Join-Path $e 'Engine\Build\BatchFiles\Build.bat'
    Editor    = Join-Path $e 'Engine\Binaries\Win64\UnrealEditor.exe'
    EditorCmd = Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    Ubt       = Join-Path $e 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
    Saved     = Join-Path $script:Repo 'unreal\DeepField\Saved'
    Binaries  = Join-Path $script:Repo 'unreal\DeepField\Binaries\Win64'
  }
}

function Test-EditorMayHoldProject {
  # UBT refuses to build while "Live Coding is active", which it detects by a mutex named after the
  # target's executable. With an installed engine that is the engine's own UnrealEditor.exe, so an open
  # editor on ANY project blocks every build on the machine: the runner's checkout, an agent worktree.
  # The guard only matters when the editor has THIS project's DLLs loaded. True when an editor has this
  # project open, or when we cannot tell (no project on its command line, or the line is unreadable).
  $mine = ([IO.Path]::GetFullPath($script:Project)).Replace('/', '\')
  $editors = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" -ErrorAction SilentlyContinue)
  foreach ($e in $editors) {
    $cmd = "$($e.CommandLine)".Replace('/', '\')
    if (-not $cmd -or $cmd -notmatch '\.uproject') { return $true }
    if ($cmd.IndexOf($mine, [StringComparison]::OrdinalIgnoreCase) -ge 0) { return $true }
  }
  return $false
}

# A build line that says why it failed: MSVC `file(12): error C2065:`, `fatal error`, linker `error LNK2019`,
# UHT `file(12): Error:`, and UBT's own refusals ("Unable to build while Live Coding is active").
$BuildErrorRe = ': (fatal )?error[ :]|error [A-Z]+\d+|Unable to build|Result: Failed|BUILD FAILED'

function Invoke-Build {
  Write-Step 'Build: DeepFieldEditor Win64 Development'
  $p = Get-Paths
  $logDir = Join-Path $p.Saved 'Logs'; New-Item -ItemType Directory -Force -Path $logDir | Out-Null
  $log = Join-Path $logDir 'build-editor.log'
  Write-Info 'The first build compiles every module and takes 10-30 minutes; later builds only what changed.'
  Write-Info "Full log: $log"
  $ubtArgs = @('DeepFieldEditor', 'Win64', 'Development', "-Project=$($script:Project)", '-WaitMutex', '-NoHotReload')
  if (Test-EditorMayHoldProject) {
    Write-Info 'An editor may have this project open: if the build stops at "Live Coding is active", close it or press Ctrl+Alt+F11 in it.'
  } else {
    # Every open editor is on another project or checkout, so the Live Coding guard is a false positive.
    $ubtArgs += '-NoHotReloadFromIDE'
  }
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try {
    & $p.BuildBat @ubtArgs 2>&1 |
      ForEach-Object { "$_" } | Tee-Object -FilePath $log | ForEach-Object {
        if ($_ -match $BuildErrorRe) { Write-Host $_ -ForegroundColor Red }
        elseif ($_ -match '^\[\d+/\d+\]|Result: Succeeded|Total execution time|Building |Using ') { Write-Host $_ }
      }
    $rc = $LASTEXITCODE
  } finally { $ErrorActionPreference = $old }
  if ($rc -ne 0) {
    # All of them together, whatever scrolled past: int-merge.sh's filter once hid clang's `file:12:34: error:`
    # lines, and recovering a message the first build had already produced cost two full rebuilds.
    $diag = @(Select-String -Path $log -Pattern $BuildErrorRe -ErrorAction SilentlyContinue | Select-Object -First 30)
    if ($diag.Count -gt 0) {
      Write-Host '   -- the diagnostics, in full:' -ForegroundColor Red
      $diag | ForEach-Object { Write-Host "      $($_.Line)" -ForegroundColor Red }
    }
    Fail "the build failed (exit $rc)" "The errors are above; the whole output is in $log.`nIf it says the compiler or SDK is missing or banned, run: deepfield doctor"
    return $false
  }
  Write-Ok 'build succeeded'
  return $true
}

function Get-GateFilter {
  # The landing gate is defined once, in test.sh; read it from there so the two never disagree.
  $sh = Join-Path $script:Repo 'unreal\Build\test.sh'
  $m = Select-String -Path $sh -Pattern '^DF_GATE_FILTER="([^"]+)"' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($m) { return $m.Matches[0].Groups[1].Value }
  return 'DF.Unit+DF.Content+DF.Online+DF.Editor+DF.UI+DF.Func'
}

function Assert-FreshBinaries {
  # test.sh's rule: a green run on a stale binary is indistinguishable from a real pass.
  $p = Get-Paths
  $bins = @(Get-ChildItem $p.Binaries -Include *.dll, *.target, *.modules -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
  if ($bins.Count -eq 0) { Fail "nothing built at $($p.Binaries)" 'Run: deepfield build'; return }
  $newestBin = $bins[0].LastWriteTime
  $src = Join-Path $script:Repo 'unreal\DeepField'
  # test.sh's list, the .uproject included: a module added or a plugin enabled there changes what gets
  # built, so a binary older than it tests a project that no longer exists (dropped by the first port).
  $newer = @(@(Get-ChildItem (Join-Path $src 'Source'), (Join-Path $src 'Config') -Recurse -File -Include *.cpp, *.h, *.inl, *.cs, *.ini -ErrorAction SilentlyContinue) +
      @(Get-Item $script:Project -ErrorAction SilentlyContinue) |
      Where-Object { $_ -and $_.LastWriteTime -gt $newestBin } | Select-Object -First 1)
  if ($newer.Count -gt 0) {
    $rel = $newer[0].FullName.Substring($script:Repo.Length + 1)
    if ($AllowStale) { Write-Warn2 "$rel is newer than the built modules; testing anyway (-AllowStale)" }
    else { Fail "$rel is newer than the built modules, so the tests would run code that is not in them" 'Build first (deepfield test builds unless the build fails), or pass -AllowStale if you mean it.' }
  }
}

function Test-EmptyAllowed([string]$Filter, [string]$Log) {
  # A filter that matches nothing is a failure (a typo reads as green otherwise). test.sh's one escape,
  # DF_TEST_ALLOW_EMPTY=1, for a suite a workstream has not written yet; documented, so it exists here too.
  if ($env:DF_TEST_ALLOW_EMPTY -eq '1') { Write-Warn2 "no test matched '$Filter'; allowed by DF_TEST_ALLOW_EMPTY=1 ($Log)"; return $true }
  Fail "no test matched '$Filter'" "A typo in the filter? (DF_TEST_ALLOW_EMPTY=1 if an empty suite is expected.) Log: $Log"
  return $false
}

function Invoke-Tests([string]$Filter) {
  if (-not $Filter) { $Filter = Get-GateFilter }
  Write-Step "Tests: $Filter"
  Assert-FreshBinaries
  $p = Get-Paths
  $safe = ($Filter -replace '[+.]', '_')
  $log = Join-Path $p.Saved "Logs\test-$safe.log"
  $report = Join-Path $p.Saved "Automation\Reports\test-$safe"
  if (Test-Path $report) { Remove-Item $report -Recurse -Force }   # a stale report must never supply the verdict
  New-Item -ItemType Directory -Force -Path $report, (Split-Path $log) | Out-Null
  $timeout = 1800; if ($env:DF_TEST_TIMEOUT) { $timeout = [int]$env:DF_TEST_TIMEOUT }
  Write-Info "headless editor, up to $([int]($timeout / 60)) minutes. Log: $log"
  $argList = @("`"$($script:Project)`"", '-nullrhi', '-unattended', '-nop4', '-nosplash', '-NoSound',
    "-ExecCmds=`"Automation RunTests $Filter; Quit`"", '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=`"$report`"", '-log', "-abslog=`"$log`"")
  # The tests run on the Null online services everywhere. A machine with the untracked EOS file
  # (unreal/PLAN/rfcs/needs-int-eos-config.md) would otherwise run DF.Online against Epic, with no
  # login, and fail where the runner and every other clone pass. DF_TEST_ONLINE_SERVICES=Epic (plus
  # -DFDeviceId in DF_TEST_EXTRA_ARGS) tests against EOS on purpose.
  $services = 'Null'; if ($env:DF_TEST_ONLINE_SERVICES) { $services = $env:DF_TEST_ONLINE_SERVICES }
  $argList += "-ini:Engine:[OnlineServices]:DefaultServices=$services"
  if ($env:DF_TEST_EXTRA_ARGS) { $argList += ($env:DF_TEST_EXTRA_ARGS -split ' ' | Where-Object { $_ }) }
  $proc = Start-Process $p.EditorCmd -ArgumentList $argList -PassThru -WindowStyle Hidden
  $null = $proc.Handle   # without a handle taken now, ExitCode reads empty after the process ends
  if (-not $proc.WaitForExit($timeout * 1000)) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Fail "the tests timed out after $timeout s" "See $log"; return $false
  }

  $index = Join-Path $report 'index.json'
  if (Test-Path $index) {
    $data = [IO.File]::ReadAllText($index, [Text.Encoding]::UTF8).TrimStart([char]0xFEFF) | ConvertFrom-Json
    $tests = @($data.tests | Sort-Object fullTestPath)
    $failed = 0
    foreach ($t in $tests) {
      if ($t.state -eq 'Success') { Write-Host "   PASS $($t.fullTestPath)" -ForegroundColor Green; continue }
      $failed++
      Write-Host "   FAIL $($t.fullTestPath)  [$($t.state)]" -ForegroundColor Red
      foreach ($e in @($t.entries)) {
        if ($e.event.type -eq 'Error' -or $e.event.type -eq 'Warning') { Write-Host "        $($e.event.type): $($e.event.message)" -ForegroundColor Red }
      }
    }
    if ($tests.Count -eq 0) { return (Test-EmptyAllowed $Filter $log) }
    if ($failed -gt 0) { Fail "$failed of $($tests.Count) tests failed" "Log: $log"; return $false }
    Write-Ok "$($tests.Count) passed"
    return $true
  }
  if (Test-Path $log) {
    $none = Select-String -Path $log -Pattern 'No automation tests matched' | Select-Object -First 1
    if ($none) { return (Test-EmptyAllowed $Filter $log) }
    Select-String -Path $log -Pattern 'Error:|Fatal|Assertion' | Select-Object -First 10 | ForEach-Object { Write-Host "   $($_.Line)" -ForegroundColor Red }
  }
  Fail "the editor exited ($($proc.ExitCode)) without a test report" "Log: $log"
  return $false
}

function Invoke-RepoChecks([object[]]$Extra = @()) {
  # $Extra: more checks, each an array of the script name and its arguments (pr-check adds ownership).
  Write-Step 'Repository checks (no engine needed)'
  $build = Join-Path $script:Repo 'unreal\Build'
  $failed = @()
  foreach ($c in (@(@('layering-check.py'), @('validate-content-json.py'), @('check-test-coverage.py')) + $Extra)) {
    Write-Host "   -- $($c -join ' ')"
    $rc = Invoke-Python (@((Join-Path $build $c[0])) + @($c | Select-Object -Skip 1))
    if ($rc -ne 0) { $failed += $c[0] }
  }
  if ($failed.Count -gt 0) { Fail "failed: $($failed -join ', ')" 'The output above says what is wrong.' ; return }
  Write-Ok 'repository checks passed'
}

function Get-WorkstreamFromBranch {
  # ws/04-towers/rig -> 04 ; ws/10a-map-foundry/x -> 10a (the same rule as pr-check.sh)
  $branch = Get-NativeOutput 'git' @('-C', $script:Repo, 'rev-parse', '--abbrev-ref', 'HEAD')
  if ($branch -and $branch -match '^ws/([0-9]+[a-z]?)-') { return $Matches[1] }
  return $null
}

function Invoke-PrCheck([string]$Filter) {
  # Build/pr-check.sh on Windows (PROGRAMME.md 6.5): what a workstream runs before opening a PR. With no
  # filter the tests are the landing gate, the same set a landing runs, so a PR never learns about a red
  # suite at landing time. A filtered run is for iterating and is reported PARTIAL, never OK: an
  # omitted suite has to be visible (CONTRACTS/ci.md).
  $wsId = $Ws
  if (-not $wsId) { $wsId = Get-WorkstreamFromBranch }
  if (-not $wsId) { Fail 'pr-check needs your workstream' 'Pass -Ws NN (for example: deepfield pr-check -Ws 04), or run it on a ws/NN-slug/topic branch.'; return $false }
  Write-Info "workstream WS-$wsId; ownership is checked against $Base"
  Invoke-RepoChecks -Extra (, @('ownership-check.py', '--ws', $wsId, '--base', $Base))
  if (-not (Invoke-Build)) { return $false }
  if (-not (Invoke-Tests $Filter)) { return $false }
  Write-Host ''
  if ($Filter) {
    Write-Host "pr-check: PARTIAL (WS-$wsId, tests: $Filter only) - run it without a filter before opening the PR" -ForegroundColor Yellow
  } else {
    Write-Host "pr-check: OK (WS-$wsId) - open the PR titled [WS-$wsId] ..., listing the contracts touched and the tests above" -ForegroundColor Green
  }
  Write-Info 'Touched anything networked? Also run the listen-host smoke by hand (unreal\README.md, 5.3).'
  return $true
}

function Read-SharedText([string]$Path) {
  # A log the engine may still be writing: open it shared, and read a missing file as empty.
  if (-not (Test-Path $Path)) { return '' }
  $fs = $null; $sr = $null
  try {
    $fs = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $sr = New-Object IO.StreamReader($fs)
    return $sr.ReadToEnd()
  } catch { return '' } finally { if ($sr) { $sr.Dispose() } elseif ($fs) { $fs.Dispose() } }
}

function Invoke-Smoke([int]$ClientCount, [int]$SmokePort) {
  # Build/smoke-listen.sh on Windows: a headless listen host and N headless clients that join it (the
  # stand-in for DF.Net.ListenHostPlusClient). The pass condition, re-derived against the join path PR #48
  # added (ADFGameMode::PreLogin asks the C14 join validators before anyone is admitted):
  #   - every client logs 'Welcomed by server'. The server sends it only after PreLogin accepted the
  #     connection. ('Bringing up level for play took' is not enough: a client that fails to connect
  #     falls back to its default map and logs that too.)
  #   - the host logs 'player joined' (ADFGameMode::PostLogin, after PreLogin) for its own player and
  #     for each client, every one of them seated (seat 1-4, never 0);
  #   - the host logs no 'join refused' (PreLogin's refusal, with its reason).
  # A bare 127.0.0.1 join is admitted by UDFOnlineSubsystem's dev-join branch: a '?listen' host is not a
  # hosted session, so there is no handshake to check. The run reports when that branch was taken.
  if ($ClientCount -lt 1 -or $ClientCount -gt 3) { Fail "the smoke takes 1 to 3 clients, not $ClientCount" 'The host takes one of the 4 seats (ADFGameMode::MaxSeats).'; return $false }
  $p = Get-Paths
  $logs = Join-Path $p.Saved 'Logs'
  New-Item -ItemType Directory -Force -Path $logs | Out-Null
  $hostLog = Join-Path $logs 'smoke-host.log'
  $clientLogs = @(1..$ClientCount | ForEach-Object { Join-Path $logs "smoke-client-$_.log" })
  Remove-Item -Path (@($hostLog) + $clientLogs) -Force -ErrorAction SilentlyContinue
  $common = @('-game', '-nullrhi', '-unattended', '-nop4', '-nosplash', '-NoSound', '-log')
  $timeout = 180; if ($env:DF_SMOKE_TIMEOUT) { $timeout = [int]$env:DF_SMOKE_TIMEOUT }
  $need = $ClientCount + 1
  Write-Step "Smoke: a listen host and $ClientCount client(s) on $Map (port $SmokePort)"
  Write-Info "headless editors, up to $timeout s for the joins. Logs: $logs\smoke-*.log"
  $procs = New-Object System.Collections.ArrayList
  try {
    $hostArgs = @("`"$($script:Project)`"", "$Map`?listen", "-port=$SmokePort") + $common + @("-abslog=`"$hostLog`"")
    [void]$procs.Add((Start-Process $p.EditorCmd -ArgumentList $hostArgs -PassThru -WindowStyle Hidden))
    $until = (Get-Date).AddSeconds(120)   # the host brings its map up first
    while ((Get-Date) -lt $until -and (Read-SharedText $hostLog) -notmatch 'LogNet: .*listening|LogWorld: Bringing up level|Game class is') { Start-Sleep -Seconds 1 }
    Start-Sleep -Seconds 5
    for ($c = 1; $c -le $ClientCount; $c++) {
      $clientArgs = @("`"$($script:Project)`"", "127.0.0.1:$SmokePort") + $common + @("-abslog=`"$($clientLogs[$c - 1])`"")
      [void]$procs.Add((Start-Process $p.EditorCmd -ArgumentList $clientArgs -PassThru -WindowStyle Hidden))
      if ($c -lt $ClientCount) { Start-Sleep -Seconds 4 }
    }
    $until = (Get-Date).AddSeconds($timeout)
    while ((Get-Date) -lt $until) {
      $h = Read-SharedText $hostLog
      if ($h -match 'join refused') { break }
      $joined = ([regex]::Matches($h, 'player joined: ')).Count
      $welcomed = @($clientLogs | Where-Object { (Read-SharedText $_) -match 'Welcomed by server' }).Count
      if ($joined -ge $need -and $welcomed -eq $ClientCount) { break }
      if (@($procs | Where-Object { $_.HasExited }).Count -gt 0) { break }   # an editor that died will not join
      Start-Sleep -Seconds 1
    }
    Start-Sleep -Seconds 5
  } finally {
    foreach ($pr in $procs) { if ($pr -and -not $pr.HasExited) { Stop-Process -Id $pr.Id -Force -ErrorAction SilentlyContinue } }
  }

  $h = Read-SharedText $hostLog
  Write-Host '   -- host'
  @($h -split "`r?`n" | Where-Object { $_ -match 'player joined|join refused|join rejected|dev join|LogNet: .*listening|Error:' } | Select-Object -First 25) | ForEach-Object { Write-Info $_.Trim() }
  $problems = @()
  foreach ($m in [regex]::Matches($h, 'join refused \(([^)]*)\)')) { $problems += "the host refused a client ($($m.Groups[1].Value))" }
  $joins = @([regex]::Matches($h, 'player joined: \S+ \(seat (\d+)\)'))
  if ($joins.Count -lt $need) { $problems += "the host saw $($joins.Count) player(s) join, needed $need (its own player + $ClientCount client(s))" }
  $unseated = @($joins | Where-Object { $_.Groups[1].Value -eq '0' }).Count
  if ($unseated -gt 0) { $problems += "$unseated player(s) joined without a seat (seat 0)" }
  for ($c = 1; $c -le $ClientCount; $c++) {
    $t = Read-SharedText $clientLogs[$c - 1]
    Write-Host "   -- client $c"
    @($t -split "`r?`n" | Where-Object { $_ -match 'Welcomed by server|Bringing up level|Failure|Error:' } | Select-Object -First 25) | ForEach-Object { Write-Info $_.Trim() }
    if ($t -notmatch 'Welcomed by server') { $problems += "client $c was never admitted (no 'Welcomed by server' in its log)" }
  }
  if ($problems.Count -gt 0) { Fail "the smoke failed: $($problems -join '; ')" "Logs: $logs\smoke-*.log"; return $false }
  $dev = ([regex]::Matches($h, 'dev join')).Count
  if ($dev -gt 0) { Write-Info "admitted through the dev-join branch ($dev): a bare IP join to a '?listen' host. A hosted session runs the full handshake." }
  Write-Ok "host + $ClientCount client(s) joined $Map, seats $(($joins | ForEach-Object { $_.Groups[1].Value }) -join ', ')"
  return $true
}

function Invoke-CiLocal([string]$Filter, [int]$ClientCount, [int]$SmokePort) {
  # Build/ci-local.sh on Windows: the INT pre-merge set (PROGRAMME.md 6.8), what the nightly lane runs and
  # what INT runs on a rebased branch before it lands. Every step in order, stop at the first failure,
  # and a summary table whatever happened. plan-status is a warning unless -Strict: claims and lease
  # renewals land on main between INT cycles by design, so the committed STATUS.md is often stale.
  $steps = @('layering', 'ownership', 'schema', 'coverage', 'plan-status', 'build', 'tests', 'smoke')
  $skipList = @($Skip | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
  foreach ($s in $skipList) { if ($steps -notcontains $s) { Fail "unknown step '$s' in -Skip" "The steps are: $($steps -join ' ')"; return $false } }
  $wsId = $Ws; if (-not $wsId) { $wsId = 'INT' }
  $build = Join-Path $script:Repo 'unreal\Build'
  $result = @{}; $secs = @{}
  foreach ($s in $steps) { $result[$s] = 'not run'; $secs[$s] = '' }
  $warned = @(); $failedStep = ''; $t0 = Get-Date
  $start = Get-Date
  try {
    foreach ($s in $steps) {
      if ($skipList -contains $s) { $result[$s] = 'skipped'; Write-Step "$($s): skipped"; continue }
      $failedStep = $s   # until it passes: a step that stops the script (Fail) is recorded as the failure
      $t0 = Get-Date
      $ok = $false
      switch ($s) {
        'layering'    { $ok = ((Invoke-Python @((Join-Path $build 'layering-check.py'))) -eq 0) }
        'ownership'   { $ok = ((Invoke-Python @((Join-Path $build 'ownership-check.py'), '--ws', $wsId, '--base', $Base)) -eq 0) }
        'schema'      { $ok = ((Invoke-Python @((Join-Path $build 'validate-content-json.py'))) -eq 0) }
        'coverage'    { $ok = ((Invoke-Python @((Join-Path $build 'check-test-coverage.py'))) -eq 0) }
        'plan-status' { $ok = ((Invoke-Python @((Join-Path $build 'plan-status.py'), '--check')) -eq 0) }
        'build'       { $ok = (Invoke-Build) }
        'tests'       { $ok = (Invoke-Tests $Filter) }
        'smoke'       { $ok = (Invoke-Smoke $ClientCount $SmokePort) }
      }
      $secs[$s] = [int]((Get-Date) - $t0).TotalSeconds
      if (-not $ok) {
        if ($s -eq 'plan-status' -and -not $Strict) {
          $result[$s] = 'WARN'; $warned += $s; $failedStep = ''
          Write-Warn2 'STATUS.md is stale (not blocking; -Strict makes it fail). Regenerate: python unreal\Build\plan-status.py, then commit it.'
          continue
        }
        $result[$s] = 'FAIL'
        Fail "ci-local failed at $s" 'The output above says what is wrong.'
        return $false
      }
      $result[$s] = 'ok'; $failedStep = ''
    }
  } catch [System.OperationCanceledException] {
    if ($failedStep) { $result[$failedStep] = 'FAIL'; if ($secs[$failedStep] -eq '') { $secs[$failedStep] = [int]((Get-Date) - $t0).TotalSeconds } }
    throw
  } finally {
    Write-Host ''
    Write-Host "ci-local summary ($([int]((Get-Date) - $start).TotalSeconds) s total)"
    Write-Host ('  {0,-12} {1,-9} {2}' -f 'step', 'result', 'seconds')
    foreach ($s in $steps) { Write-Host ('  {0,-12} {1,-9} {2}' -f $s, $result[$s], $secs[$s]) }
    if ($failedStep) { Write-Host "ci-local: FAILED at $failedStep" -ForegroundColor Red }
    elseif ($Filter -and $skipList -notcontains 'tests') { Write-Host "ci-local: PARTIAL (tests: $Filter only, not the landing gate)" -ForegroundColor Yellow }
    elseif ($warned.Count -gt 0) { Write-Host "ci-local: OK with warnings ($($warned -join ', '))" -ForegroundColor Yellow }
    elseif ($skipList.Count -gt 0) { Write-Host "ci-local: OK, with $($skipList -join ', ') skipped" -ForegroundColor Yellow }
    else { Write-Host 'ci-local: OK' -ForegroundColor Green }
  }
  return $true
}

# ---------------------------------------------------------------------------------------------------
# int-merge: land one workstream branch on main (Build/int-merge.sh on Windows; PROGRAMME.md 6.8)
# ---------------------------------------------------------------------------------------------------
# Ported rule by rule, not line by line (the WS-15 brief of 2026-09-25): each guard says what went wrong
# without it. Read that before deciding one is incidental.
#
# Paths whose change on main never invalidates a build or test verdict: the ledger, docs and the ledger
# scripts. The cheap checks still re-run on any movement, because OWNERSHIP.md is among these paths and
# the ownership verdict depends on it. deepfield.ps1 is deliberately absent: it decides how the build
# and the tests run.
$DocOnlyRe = '^(unreal/PLAN/|docs/|README\.md$|.*\.md$|unreal/Build/(int-merge\.sh|plan-[a-z]+\.py|plan_lib\.py|merge-ws-log\.py)$)'
# The only conflicts a landing resolves itself: STATUS.md is generated, and a workstream file's dated
# sections are append-only, which merge-ws-log.py unions. Everything else stops for a human. Widening
# this is how someone's work is lost to a merge nobody read.
$LedgerConflictRe = '^unreal/PLAN/(workstreams/ws-[0-9a-z-]+\.md|STATUS\.md)$'

function Invoke-Git([string[]]$GitArgs) {
  # git in the current location: the exit code, the output discarded.
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try { & git @GitArgs 2>&1 | Out-Null; return $LASTEXITCODE } finally { $ErrorActionPreference = $old }
}

function Get-GitLines([string[]]$GitArgs) {
  # git's stdout as lines, empty ones dropped; nothing when git fails.
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try {
    $out = @(& git @GitArgs 2>$null)
    if ($LASTEXITCODE -ne 0) { return @() }
    return @($out | ForEach-Object { "$_" } | Where-Object { $_ -ne '' })
  } finally { $ErrorActionPreference = $old }
}

function Get-GitOne([string[]]$GitArgs) { return (@(Get-GitLines $GitArgs) | Select-Object -First 1) }

function Stop-Landing([string]$What) {
  Fail "int-merge failed at: $What" "The verify worktree is left at $($script:IM.Wt) on $($script:IM.IntBranch). Fix it there and rerun with -Resume, or rerun without it to start over."
}

function Enter-LandingLock([string]$LockPath, [string]$What) {
  # Guard: one landing at a time. Two landings in one verify worktree swap the tree under a running test;
  # that happened, and only the staleness guard caught it. int-merge.sh used a mkdir lock holding a pid,
  # plus a 60 s grace for a holder that crashed before writing it. Here the lock is an open handle with no
  # sharing, which Windows releases when the holding process exits, however it exits: a crashed landing
  # cannot leave a stale lock, so nothing has to be judged from pids and ages. The holder's pid and branch
  # sit beside it only for the waiting message.
  $holderFile = "$LockPath.holder"
  $max = 7200; if ($env:DF_INT_LOCK_MAX) { $max = [int]$env:DF_INT_LOCK_MAX }
  $waited = 0
  while ($true) {
    try {
      $script:LandingLock = [IO.File]::Open($LockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
      Set-Content -LiteralPath $holderFile -Encoding ascii -Value "pid $PID, $What"
      return
    } catch [System.IO.IOException] {
      $holder = ''; try { $holder = Get-Content -LiteralPath $holderFile -TotalCount 1 -ErrorAction Stop } catch { }
      if ($waited -ge $max) { Fail "another landing ($holder) has held $LockPath for $max s" 'Wait for it or stop it; the lock frees itself when that process exits.'; return }
      if ($waited -eq 0) { Write-Info "waiting for the landing in progress ($holder)" }
      Start-Sleep -Seconds 30; $waited += 30
    }
  }
}

function Exit-LandingLock { if ($script:LandingLock) { $script:LandingLock.Dispose(); $script:LandingLock = $null } }

function Test-InRebase {
  foreach ($d in 'rebase-merge', 'rebase-apply') {
    $p = Get-GitOne @('rev-parse', '--git-path', $d)
    if ($p -and (Test-Path -LiteralPath $p)) { return $true }
  }
  return $false
}

function Get-LandingBranch {
  # The branch even mid-rebase, when HEAD is detached and only the rebase state names it.
  foreach ($d in 'rebase-merge', 'rebase-apply') {
    $p = Get-GitOne @('rev-parse', '--git-path', "$d/head-name")
    if ($p -and (Test-Path -LiteralPath $p)) { return ((Get-Content -LiteralPath $p -TotalCount 1).Trim() -replace '^refs/heads/', '') }
  }
  $b = Get-GitOne @('rev-parse', '--abbrev-ref', 'HEAD')
  if ($b) { return $b }
  return '?'
}

function Invoke-RebaseOntoMain {
  # Rebase onto origin/main, or finish a rebase already in progress, until HEAD sits on top of main. One
  # pass is not enough: a fetch may move main while a human is resolving. True when done; false, after
  # saying why, when a human is needed.
  $guard = 0
  while ($true) {
    if (-not (Test-InRebase)) {
      if ((Invoke-Git @('merge-base', '--is-ancestor', 'origin/main', 'HEAD')) -eq 0) { return $true }
      [void](Invoke-Git @('rebase', '-q', 'origin/main'))
      if (-not (Test-InRebase)) {
        if ((Invoke-Git @('merge-base', '--is-ancestor', 'origin/main', 'HEAD')) -eq 0) { return $true }
        Write-Warn2 'git rebase refused to start (or stopped without conflicts):'
        Get-GitLines @('status', '--short') | Select-Object -First 5 | ForEach-Object { Write-Info $_ }
        return $false
      }
    }
    $guard++
    if ($guard -gt 200) { Write-Warn2 'rebase: 200 steps without finishing; stopping'; return $false }

    $conflicted = @(Get-GitLines @('diff', '--name-only', '--diff-filter=U'))
    if ($conflicted.Count -gt 0) {
      if (@($conflicted | Where-Object { $_ -notmatch $LedgerConflictRe }).Count -gt 0) {
        Write-Warn2 'conflicts left for a human (resolve and STAGE them in the verify worktree, then rerun with -Resume):'
        $conflicted | ForEach-Object { Write-Info $_ }
        return $false
      }
      foreach ($f in $conflicted) {
        if ($f -eq 'unreal/PLAN/STATUS.md') {
          Write-Info 'STATUS.md conflict: regenerated'
          if ((Invoke-Python @('unreal/Build/plan-status.py')) -ne 0 -or (Invoke-Git @('add', '--', $f)) -ne 0) { Write-Warn2 'could not regenerate STATUS.md'; return $false }
        } else {
          # The clone's merge-ws-log.py, not the branch's: a branch older than the tool does not carry it.
          if ((Invoke-Python @((Join-Path $PSScriptRoot 'merge-ws-log.py'), $f)) -ne 0) { Write-Warn2 "could not union-merge $f"; return $false }
        }
      }
    }

    # Guard: never continue or skip over unstaged or untracked work. We cannot tell what the human meant,
    # and --skip on a merely-unstaged resolution drops the author's commit and force-pushes the result.
    if ((Invoke-Git @('diff', '--quiet')) -ne 0 -or @(Get-GitLines @('ls-files', '--others', '--exclude-standard')).Count -gt 0) {
      Write-Warn2 "unstaged or untracked changes in $($script:IM.Wt): stage what belongs in the commit, discard the rest, then rerun with -Resume"
      Get-GitLines @('status', '--short') | Select-Object -First 10 | ForEach-Object { Write-Info $_ }
      return $false
    }

    # Guard: `git rebase --continue` exits non-zero when it commits and then stops at the NEXT conflict,
    # the normal case here and not a failure (treating it as one made a two-conflict branch unlandable).
    # The loop re-inspects; only a step that changes nothing at all is stuck.
    $before = Get-GitOne @('rev-parse', 'HEAD')
    if ((Invoke-Git @('diff', '--cached', '--quiet')) -eq 0) {
      Write-Info 'a commit became empty after the rebase; skipping it'
      [void](Invoke-Git @('rebase', '--skip'))
    } else {
      [void](Invoke-Git @('rebase', '--continue'))
    }
    if ((Test-InRebase) -and (Get-GitOne @('rev-parse', 'HEAD')) -eq $before -and @(Get-GitLines @('diff', '--name-only', '--diff-filter=U')).Count -eq 0) {
      Write-Warn2 'rebase is stuck: nothing conflicted, nothing applied. git says:'
      Get-GitLines @('status') | Select-Object -First 12 | ForEach-Object { Write-Info $_ }
      return $false
    }
  }
}

function Save-Verified {
  # Guard: the verification record pins branch, base, HEAD, smoke and workstream, and the base is the
  # merge-base, never origin/main: refs are shared by every worktree of the clone and any session's fetch
  # moves them mid-build, which would make "main did not move" a lie. Recording the ref cost one landing
  # three rejected pushes.
  $im = $script:IM
  $im.VerifiedBase = Get-GitOne @('merge-base', 'HEAD', 'origin/main')
  $head = Get-GitOne @('rev-parse', 'HEAD')
  Set-Content -LiteralPath $im.VbFile -Encoding ascii -Value ('{0} {1} {2} {3} {4}' -f $im.Branch, $im.VerifiedBase, $head, [int]$im.Smoke, $im.WsId)
}

function Save-BranchTip { Set-Content -LiteralPath $script:IM.TipFile -Encoding ascii -Value "$($script:IM.Branch) $($script:IM.BranchTip)" }

function Read-Record([string]$Path) {
  if (Test-Path -LiteralPath $Path) { return @(("$(Get-Content -LiteralPath $Path -TotalCount 1)").Trim() -split ' ') }
  return @()
}

function Save-StatusCommit {
  # STATUS.md for the state about to land, as its own commit when more than its timestamp moved.
  [void](Invoke-Python @('unreal/Build/plan-status.py'))
  if ((Invoke-Git @('diff', '--quiet', '--', 'unreal/PLAN/STATUS.md')) -eq 0) { return }
  $changed = @(Get-GitLines @('diff', '-U0', '--', 'unreal/PLAN/STATUS.md') | Where-Object { $_ -match '^[+-]' -and $_ -notmatch '^(\+\+\+|---) ' })
  if (@($changed | Where-Object { $_ -notmatch '^[+-]Generated ' }).Count -eq 0) {
    [void](Invoke-Git @('checkout', '-q', '--', 'unreal/PLAN/STATUS.md'))   # only the timestamp moved: not a commit
    return
  }
  if ((Invoke-Git @('add', '--', 'unreal/PLAN/STATUS.md')) -ne 0 -or (Invoke-Git @('commit', '-q', '-m', "PLAN: STATUS regenerated after landing $($script:IM.Branch)")) -ne 0) {
    Stop-Landing 'committing the regenerated STATUS.md'
  }
  Write-Info 'STATUS.md regenerated (its own commit)'
}

function Get-OtherUbtCount {
  # Real UBT processes only: dotnet running UnrealBuildTool.dll. int-merge.sh's first try, pgrep -f, also
  # counted every shell whose command line merely mentioned the name (other sessions watch the queue so).
  return @(Get-CimInstance Win32_Process -Filter "Name = 'dotnet.exe' OR Name = 'UnrealBuildTool.exe'" -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -eq 'UnrealBuildTool.exe' -or "$($_.CommandLine)" -match 'UnrealBuildTool\.dll' }).Count
}

function Wait-OtherBuilds {
  # Guard: yield to other builds before starting one. Kept on the GPU box, deliberately (2026-09-26): the
  # Mac's 8 GB ceiling is gone, but the box is shared by the agent sessions and the runner, and UBT's
  # -WaitMutex is not FIFO, so a landing that merely queued could jump an agent's build, the critical path.
  $max = 5400; if ($env:DF_INT_YIELD_MAX) { $max = [int]$env:DF_INT_YIELD_MAX }
  $n = Get-OtherUbtCount
  if ($n -eq 0) { return }
  Write-Info "yielding: $n other UnrealBuildTool process(es) running or queued (up to $max s)"
  $w = 0
  while ((Get-OtherUbtCount) -gt 0 -and $w -lt $max) { Start-Sleep -Seconds 30; $w += 30 }
  Write-Info "yielded $w s; $(Get-OtherUbtCount) other build(s) left"
}

function Invoke-LandingChecks {
  # The branch's own checks, from the verify worktree, with ownership judged against origin/main.
  Write-Step 'checks'
  $checks = @(@('layering-check.py'), @('ownership-check.py', '--ws', $script:IM.WsId, '--base', 'origin/main'),
    @('validate-content-json.py'), @('check-test-coverage.py'))
  foreach ($c in $checks) {
    Write-Host "   -- $($c -join ' ')"
    if ((Invoke-Python (@("unreal/Build/$($c[0])") + @($c | Select-Object -Skip 1))) -ne 0) { Stop-Landing $c[0] }
  }
  Write-Ok 'checks passed'
}

function Invoke-LandingVerify {
  Invoke-LandingChecks
  Wait-OtherBuilds
  # Invoke-Build prints every diagnostic line on a failure (the next guard in the brief).
  if (-not (Invoke-Build)) { Stop-Landing 'build' }
  if (-not (Invoke-Tests '')) { Stop-Landing 'tests' }   # no filter: test.sh's DF_GATE_FILTER, the landing gate
  if ($script:IM.Smoke) { if (-not (Invoke-Smoke $script:IM.Clients $script:IM.SmokePort)) { Stop-Landing 'smoke' } }
  Save-Verified
}

function Test-DocOnlyMove([string]$From, [string]$To) {
  # True when only ledger/doc paths differ between two commits; false when git cannot say.
  if ((Invoke-Git @('cat-file', '-e', "$From^{commit}")) -ne 0 -or (Invoke-Git @('cat-file', '-e', "$To^{commit}")) -ne 0) { return $false }
  $files = @(Get-GitLines @('diff', '--name-only', $From, $To))
  return (@($files | Where-Object { $_ -notmatch $DocOnlyRe }).Count -eq 0)
}

function Start-Landing {
  $im = $script:IM
  Remove-Item -LiteralPath $im.VbFile, $im.TipFile -ErrorAction SilentlyContinue
  if (Test-InRebase) { [void](Invoke-Git @('rebase', '--abort')) }
  # Tracked and untracked files reset; ignored Intermediate, Binaries and Saved stay, so builds are incremental.
  [void](Invoke-Git @('reset', '-q', '--hard'))
  [void](Invoke-Git @('clean', '-qfd'))
  if ((Invoke-Native 'git' @('checkout', '-q', '-B', $im.IntBranch, "origin/$($im.Branch)")) -ne 0) { Stop-Landing "checkout origin/$($im.Branch) (does the branch exist?)" }
  # Guard: the branch push's lease is the tip this landing took, persisted for -Resume, never the ref a
  # later fetch refreshed. Without it a landing silently overwrites an author who pushed during
  # verification. It fired for real, and refused, correctly.
  $im.BranchTip = Get-GitOne @('rev-parse', "origin/$($im.Branch)")
  Save-BranchTip
  Write-Step 'rebase onto origin/main'
  if (-not (Invoke-RebaseOntoMain)) { Stop-Landing 'rebase' }
  [void](Invoke-Native 'git' @('log', '--oneline', 'origin/main..HEAD'))
  Invoke-LandingVerify
}

function Resume-Landing {
  $im = $script:IM
  $cur = Get-LandingBranch
  if ($cur -ne $im.IntBranch) { Stop-Landing "resume: the verify worktree holds '$cur', not $($im.IntBranch) (rerun without -Resume)" }
  $tip = Read-Record $im.TipFile
  if ($tip.Count -lt 2 -or $tip[0] -ne $im.Branch -or -not $tip[1]) { Stop-Landing "resume: no record of which origin/$($im.Branch) tip this worktree took (rerun without -Resume)" }
  $im.BranchTip = $tip[1]
  if (Test-InRebase) {
    Write-Info "finishing the rebase left in $($im.Wt)"
  } else {
    # Nothing in flight: what gets verified must be exactly what gets pushed, so no uncommitted or
    # untracked work may ride along (it would pass the build and never reach main).
    if ((Invoke-Git @('diff', '--quiet')) -ne 0 -or (Invoke-Git @('diff', '--cached', '--quiet')) -ne 0) {
      Get-GitLines @('status', '--short') | Select-Object -First 10 | ForEach-Object { Write-Info $_ }
      Stop-Landing 'resume: uncommitted changes in the verify worktree; commit them (they are then verified) or discard them'
    }
    $untracked = @(Get-GitLines @('ls-files', '--others', '--exclude-standard'))
    if ($untracked.Count -gt 0) { Stop-Landing "resume: untracked files in the verify worktree: $(($untracked | Select-Object -First 5) -join ' ')" }
  }
  if (-not (Invoke-RebaseOntoMain)) { Stop-Landing 'rebase' }
  $curTip = Get-GitOne @('rev-parse', "origin/$($im.Branch)")
  if ($curTip -ne $im.BranchTip) {
    if ($curTip -and (Invoke-Git @('merge-base', '--is-ancestor', $curTip, 'HEAD')) -eq 0) {
      Write-Info "origin/$($im.Branch) moved to $($curTip.Substring(0, 7)) and this worktree already contains it; lease updated"
      $im.BranchTip = $curTip; Save-BranchTip
    } else {
      Stop-Landing "origin/$($im.Branch) moved from $($im.BranchTip.Substring(0, 7)) and this worktree does not contain the new commits (its author pushed); rerun without -Resume"
    }
  }
  $vb = Read-Record $im.VbFile
  $head = Get-GitOne @('rev-parse', 'HEAD')
  if ($vb.Count -ge 5 -and $vb[0] -eq $im.Branch -and $vb[2] -eq $head -and $vb[4] -eq $im.WsId -and ($vb[3] -eq '1' -or -not $im.Smoke)) {
    $im.VerifiedBase = $vb[1]
    Write-Info "resuming $($im.IntBranch): HEAD $($head.Substring(0, 7)) was verified against $($vb[1].Substring(0, 7))"
  } else {
    Write-Info "resuming $($im.IntBranch): no verification record for this HEAD and these settings; verifying"
    [void](Invoke-Native 'git' @('log', '--oneline', 'origin/main..HEAD'))
    Invoke-LandingVerify
  }
}

function Publish-Landing {
  $im = $script:IM
  Write-Step 'land'
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    if ((Invoke-Git @('fetch', '-q', 'origin')) -ne 0) { Stop-Landing 'fetch' }
    $main = Get-GitOne @('rev-parse', 'origin/main')
    if ($main -ne $im.VerifiedBase) {
      # Guard: ledger/doc-only movement re-runs the cheap checks and rebases without rebuilding; code or
      # content movement verifies again.
      if (Test-DocOnlyMove $im.VerifiedBase $main) {
        Write-Info 'main moved by ledger/doc-only commits since verification: rebasing and re-running the checks, no rebuild'
        if (-not (Invoke-RebaseOntoMain)) { Stop-Landing 'rebase onto the moved main' }
        Invoke-LandingChecks
        Save-Verified
      } else {
        Write-Info 'main moved with code or content since verification: verifying again'
        if (-not (Invoke-RebaseOntoMain)) { Stop-Landing 'rebase onto the moved main' }
        Invoke-LandingVerify
      }
    }
    Save-StatusCommit
    Save-Verified
    if ((Invoke-Native 'git' @('push', '-q', 'origin', "HEAD:$($im.Branch)", "--force-with-lease=$($im.Branch):$($im.BranchTip)")) -ne 0) {
      Stop-Landing "push branch: origin/$($im.Branch) moved since this landing took it at $($im.BranchTip.Substring(0, 7)) (its author pushed?); rerun without -Resume to take the new tip"
    }
    $im.BranchTip = Get-GitOne @('rev-parse', 'HEAD'); Save-BranchTip
    if ((Invoke-Git @('push', '-q', 'origin', 'HEAD:main')) -eq 0) {
      Remove-Item -LiteralPath $im.VbFile, $im.TipFile -ErrorAction SilentlyContinue
      return
    }
    Write-Info "push to main rejected (attempt $attempt); refetching"
  }
  Stop-Landing 'push main after 3 attempts (rerun with -Resume)'
}

function Invoke-IntMerge([string]$Target, [int]$ClientCount, [int]$SmokePort) {
  if (-not $Target) { Fail 'int-merge needs the branch to land' 'Example: deepfield int-merge ws/04-towers/rig -Ws 04'; return $false }
  $Target = $Target -replace '^origin/', ''
  $clone = $script:Repo
  # Beside the clone by default, so its DefaultEngine.ini DDC (../../../DDC) is the clone's too, and the
  # path stays short. int-merge.sh's /Volumes/Toshiba path has no Windows analogue.
  $wt = $VerifyDir
  if (-not $wt) { $wt = $env:DF_INT_VERIFY_WT }
  if (-not $wt) { $wt = Join-Path (Split-Path $clone -Parent) 'int-verify' }
  $wt = [IO.Path]::GetFullPath($wt)
  $wsId = $Ws; if (-not $wsId) { $wsId = 'INT' }
  $script:IM = @{
    Branch = $Target; IntBranch = "int/$Target"; Wt = $wt; WsId = $wsId; Smoke = (-not $NoSmoke)
    Clients = $ClientCount; SmokePort = $SmokePort
    VbFile = "$wt.verified-base"; TipFile = "$wt.branch-tip"; BranchTip = ''; VerifiedBase = ''
  }
  $env:GIT_EDITOR = 'true'   # rebase --continue must never wait for an editor
  $savedRepo = $script:Repo; $savedProject = $script:Project
  $inWorktree = $false
  Enter-LandingLock "$wt.lock" "landing $Target"
  try {
    Write-Step 'fetch'
    if ((Invoke-Native 'git' @('-C', $clone, 'fetch', '-q', 'origin')) -ne 0) { Stop-Landing 'fetch' }
    if (-not (Test-Path -LiteralPath (Join-Path $wt '.git'))) {
      [void](Invoke-Native 'git' @('-C', $clone, 'worktree', 'prune'))
      if ((Invoke-Native 'git' @('-C', $clone, 'worktree', 'add', '-q', '--detach', $wt, 'origin/main')) -ne 0) { Stop-Landing "worktree add $wt" }
      Write-Info "created the verify worktree at $wt (its first build compiles every module)"
    }
    Push-Location -LiteralPath $wt; $inWorktree = $true
    $script:Repo = $wt; $script:Project = Join-Path $wt 'unreal\DeepField\DeepField.uproject'
    if ($Resume) { Resume-Landing } else { Start-Landing }
    if ($DryRun) {
      Write-Host ''
      Write-Host "int-merge: dry run, verified and not pushed. The verify worktree is at $wt on $($script:IM.IntBranch)" -ForegroundColor Yellow
      return $true
    }
    Publish-Landing
    Pop-Location; $inWorktree = $false
    # The clone follows main when it is on main and clean; anything else is someone's work, left alone.
    # Say which of those it is, so "not touching it" is never a puzzle.
    $cloneBranch = Get-NativeOutput 'git' @('-C', $clone, 'symbolic-ref', '-q', '--short', 'HEAD')
    $dirty = Get-NativeOutput 'git' @('-C', $clone, 'status', '--porcelain', '--untracked-files=no')
    if (-not $cloneBranch) { Write-Info 'the clone has a detached HEAD; left as it is' }
    elseif ($cloneBranch -ne 'main') { Write-Info "the clone is on $cloneBranch, not main; left as it is (git switch main to follow the landing)" }
    elseif ($dirty) { Write-Info 'the clone is on main with uncommitted changes; left as it is (commit or stash, then git merge --ff-only origin/main)' }
    elseif ((Invoke-Native 'git' @('-C', $clone, 'merge', '-q', '--ff-only', 'origin/main')) -ne 0) { Write-Info 'the clone has local commits on main; not fast-forwarded' }
    else { Write-Info "the clone's main fast-forwarded to $(Get-NativeOutput 'git' @('-C', $clone, 'rev-parse', '--short', 'HEAD'))" }
    Write-Host ''
    Write-Host "int-merge: landed $Target on main" -ForegroundColor Green
    return $true
  } catch [System.OperationCanceledException] {
    Write-Info "int-merge stopped; the verify worktree is left at $wt on $($script:IM.IntBranch)"
    throw
  } finally {
    if ($inWorktree) { Pop-Location }
    $script:Repo = $savedRepo; $script:Project = $savedProject
    Exit-LandingLock
  }
}

function Start-Game([string[]]$Extra, [string]$What) {
  $p = Get-Paths
  $log = Join-Path $p.Saved "Logs\$What.log"
  Write-Step "Starting: $What"
  $argList = @("`"$($script:Project)`"") + $Extra + @('-game', '-windowed', '-ResX=1600', '-ResY=900', '-log', "-abslog=`"$log`"")
  Start-Process $p.Editor -ArgumentList $argList | Out-Null
  Write-Ok "started (the first start compiles shaders and takes a while). Log: $log"
}

function Invoke-Solution {
  Write-Step 'Visual Studio solution'
  $p = Get-Paths
  if (Test-Path $p.Ubt) { $rc = Invoke-Native $p.Ubt @('-projectfiles', "-project=$($script:Project)", '-game', '-rocket', '-progress') }
  else { $rc = Invoke-Native $p.BuildBat @('-projectfiles', "-project=$($script:Project)", '-game', '-rocket', '-progress') }
  if ($rc -ne 0) { Fail "generating the solution failed (exit $rc)" ''; return }
  Write-Ok "generated $(Join-Path $script:Repo 'unreal\DeepField\DeepField.sln')"
}

# ---------------------------------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------------------------------
if ($Command -eq 'help') { Get-Help $PSCommandPath -Detailed; exit 0 }

$logRoot = Join-Path $env:LOCALAPPDATA 'DeepField'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$transcript = Join-Path $logRoot "deepfield-$Command.log"
try { Start-Transcript -Path $transcript -Force | Out-Null } catch { }

$exitCode = 0
try {
  Write-Host "Deep Field 3D (Unreal) - $Command" -ForegroundColor Cyan
  $needsFullSetup = @('setup', 'doctor') -contains $Command
  $allChecks = {
    Assert-Windows
    Assert-Git
    Assert-Python
    Assert-Repo
    Read-EngineAssociation
    Assert-Engine
    Assert-VisualStudio   # after the engine: its Windows_SDK.json says which compilers it accepts
  }
  if ($needsFullSetup) {
    # Pass 1, both commands: check everything and change nothing.
    $script:DoctorOnly = $true
    Write-Host 'Checking what this machine already has. Nothing is installed or changed during this pass.'
    & $allChecks
    $missing = @($script:Problems)
    if ($Command -eq 'setup') {
      Write-Host ''
      Write-Host '== Summary' -ForegroundColor Cyan
      Write-Host "   already in place: $($script:OkCount)" -ForegroundColor Green
      if ($missing.Count -eq 0) {
        Write-Host '   missing: nothing' -ForegroundColor Green
      } else {
        Write-Host "   missing or needing a change: $($missing.Count)" -ForegroundColor Yellow
        foreach ($m in $missing) { Write-Host "     - $m" -ForegroundColor Yellow }
        Write-Host '   setup installs or changes only these. Everything marked OK is left as it is.'
        if (-not $Yes) {
          $answer = Read-Host '   Go ahead? [Y/n]'
          if ($answer -match '^[nN]') { Write-Host 'Nothing was changed.'; $script:Problems.Clear(); throw [System.OperationCanceledException]::new('declined') }
        }
      }
      # Pass 2: the same checks, now fixing what pass 1 found. Items already OK print nothing.
      $script:Problems.Clear(); $script:DoctorOnly = $false; $script:Quiet = $true
      if ($missing.Count -gt 0) { Write-Host ''; Write-Host '== Installing and fixing' -ForegroundColor Cyan }
      & $allChecks
      $script:Quiet = $false
    }
  } else {
    # The other commands check only what they use, quickly, and point at `setup` for anything missing.
    $script:Repo = @(Find-ExistingClones) | Select-Object -First 1
    if (-not $script:Repo) { Fail 'no clone found' 'Run: deepfield setup' }
    $script:Project = Join-Path $script:Repo 'unreal\DeepField\DeepField.uproject'
    if (@('check', 'pr-check', 'ci-local', 'int-merge') -contains $Command) {
      $script:Python = Find-Python
      if (-not $script:Python) { Fail 'Python is not installed' 'Run: deepfield setup' }
    }
    if (@('pr-check', 'ci-local', 'int-merge') -contains $Command) {
      # The ownership check runs git from Python, so git must be on PATH for this process and its
      # children; use an installed-but-off-PATH Git (as setup does) rather than failing.
      if (-not (Get-Command git -ErrorAction SilentlyContinue)) { $off = Find-GitOffPath; if ($off) { $env:Path = "$off;$env:Path" } }
      if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail 'Git is not installed' 'Run: deepfield setup' }
    }
    if ($Command -ne 'check') {
      Read-EngineAssociation
      $script:Engine = Find-Engine $script:EngineAssociation
      if (-not $script:Engine) { Fail "Unreal Engine $($script:EngineAssociation) is not installed" 'Run: deepfield setup' }
      if (@('build', 'test', 'pr-check', 'smoke', 'ci-local', 'int-merge', 'editor', 'play', 'host', 'join') -contains $Command -and -not (Get-VsVerdict | Where-Object { $_.Good.Count -gt 0 })) {
        Fail 'Visual Studio with an MSVC toolset UE 5.8 accepts is not installed' 'Run: deepfield setup'
      }
    }
  }

  # play and host start the first playable unless -Map names another map.
  $playMap = if ($PSBoundParameters.ContainsKey('Map')) { $Map } else { '/Game/DF/Maps/Testlane/L_Testlane' }
  # The smoke's own port, so it never collides with a `deepfield host` on 7777; -Port overrides it.
  $smokePort = 7788; if ($PSBoundParameters.ContainsKey('Port')) { $smokePort = $Port }
  switch ($Command) {
    'doctor' {
      Write-Host ''
      if ($script:Problems.Count -eq 0) { Write-Host 'doctor: everything is in place. Next: deepfield build' -ForegroundColor Green }
      else {
        Write-Host "doctor: $($script:Problems.Count) problem(s). 'deepfield setup' fixes all of them except where it says otherwise above." -ForegroundColor Yellow
        $exitCode = 1
      }
    }
    'setup' {
      if (-not $NoBuild) { [void](Invoke-Build) }
      Write-Host ''
      Write-Host 'setup: done. This machine can build and run Deep Field 3D.' -ForegroundColor Green
      Write-Host "  deepfield editor   open the Unreal editor"
      Write-Host "  deepfield play     run the game in a window"
      Write-Host "  deepfield test     run the automated tests"
      Write-Host "  deepfield help     everything else"
      Write-Host "  (deepfield.cmd is in $(Join-Path $script:Repo 'unreal'))"
    }
    'build'    { [void](Invoke-Build) }
    'test'     { if (Invoke-Build) { [void](Invoke-Tests $Arg) } }
    'pr-check' { [void](Invoke-PrCheck $Arg) }
    'smoke'    { if (Invoke-Build) { [void](Invoke-Smoke $Clients $smokePort) } }
    'ci-local' { [void](Invoke-CiLocal -Filter $Arg -ClientCount $Clients -SmokePort $smokePort) }
    'int-merge' { [void](Invoke-IntMerge -Target $Arg -ClientCount $Clients -SmokePort $smokePort) }
    'check'    { Invoke-RepoChecks }
    'editor'   {
      if (Invoke-Build) {
        $p = Get-Paths
        $editorArgs = @("`"$($script:Project)`"")
        if ($Mcp) {
          # Enabled on the command line, not in DeepField.uproject: the plugin is experimental and has a
          # Runtime module, so CI and packaged builds stay without it. The server has no authentication;
          # the engine's HTTP server binds to localhost unless [HTTPServer.Listeners] says otherwise.
          $editorArgs += "-EnablePlugins=$($McpPlugins -join ',')"
          $editorArgs += "-ExecCmds=`"ModelContextProtocol.StartServer $McpPort`""
        }
        Start-Process $p.Editor -ArgumentList $editorArgs | Out-Null
        Write-Ok 'editor starting (the first open compiles shaders: slow once, fast afterwards)'
        if ($Mcp) { Write-Info "Unreal MCP server: http://localhost:$McpPort/mcp once the editor has loaded (toolsets: $(($McpPlugins | Select-Object -Skip 1) -join ', '))" }
      }
    }
    'play'     { if (Invoke-Build) { Start-Game (@($playMap) + $(if ($Demo) { @('-DFDemo') } else { @() })) 'play' } }
    'host'     {
      if (Invoke-Build) {
        Start-Game @("$playMap`?listen", "-port=$Port") 'host'
        $ips = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' } | ForEach-Object { $_.IPAddress })
        Write-Info "Others join with: deepfield join $(if ($ips.Count) { $ips[0] } else { '<this machine''s IP>' })$(if ($Port -ne 7777) { " -Port $Port" })"
        Write-Info 'Windows Firewall may ask to allow UnrealEditor on private networks: allow it.'
      }
    }
    'join'     {
      if (-not $Arg) { Fail 'join needs the host address' 'Example: deepfield join 192.168.1.20' }
      if (Invoke-Build) { Start-Game @("$($Arg):$Port") 'join' }
    }
    'solution' { Invoke-Solution }
  }
  if ($script:Problems.Count -gt 0 -and $exitCode -eq 0) { $exitCode = 1 }
} catch [System.OperationCanceledException] {
  if ($_.Exception.Message -eq 'declined') { $exitCode = 0 }
  else {
    Write-Host ''
    Write-Host "Stopped: $($_.Exception.Message). Fix that (see above) and run the same command again." -ForegroundColor Red
    $exitCode = 1
  }
} catch {
  Write-Host ''
  Write-Host "Unexpected error: $($_.Exception.Message)" -ForegroundColor Red
  Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray
  Write-Host "Please report it with the log: $transcript" -ForegroundColor Red
  $exitCode = 2
} finally {
  try { Stop-Transcript | Out-Null } catch { }
  if ($Pause) { Write-Host ''; Read-Host 'Press Enter to close' | Out-Null }
}
exit $exitCode
