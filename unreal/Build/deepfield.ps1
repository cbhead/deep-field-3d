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
    check    The fast repository checks (Python, no engine): layering, content schemas, test coverage.
    editor   Build, then open the Unreal editor on the project.
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
  deepfield play -Map /Game/DF/Maps/Testlane/L_Testlane
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [ValidateSet('setup', 'doctor', 'build', 'test', 'check', 'editor', 'play', 'host', 'join', 'solution', 'help')]
  [string]$Command = 'setup',
  # test: the automation filter (default: the landing gate from test.sh). join: the host address.
  [Parameter(Position = 1)]
  [string]$Arg = '',
  # Where to clone when the script is not inside a clone. Default D:\DF\deepfield-3d, or C:\DF\deepfield-3d without a D: drive.
  [string]$Dir = '',
  # The engine folder (the one containing Engine\). Default: found through the launcher's records.
  [string]$EngineDir = '',
  [string]$Map = '/Game/DF/Dev/L_Dev_Empty',
  # setup: the branch to clone (default unreal/main), e.g. a PR branch to try before it merges.
  [string]$Branch = '',
  [int]$Port = 7777,
  # setup: keep the light clone (skip Megascans and the art/lighting sublevels, as the Mac does).
  [switch]$LightClone,
  # setup: do not build at the end.
  [switch]$NoBuild,
  # test: run even if the built modules are older than the source (prints what it ignores).
  [switch]$AllowStale,
  # setup: do not ask before installing what the check found missing.
  [switch]$Yes,
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
$RepoBranch    = 'unreal/main'
$EnginePatch   = 2          # UE 5.8.2: a different patch re-saves assets on open (Build/windows-bringup.md 1)
$MinFreeGB     = 150
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

function Test-MsvcAccepted([version]$v) {
  # 'preferred', 'allowed' (UBT warns and builds) or 'banned' (UBT refuses), per Get-MsvcRules.
  $rules = Get-MsvcRules
  if ($rules.Min -and $v -lt $rules.Min) { return 'banned' }
  foreach ($r in $rules.Banned) { if ($v -ge $r.Lo -and $v -le $r.Hi) { return 'banned' } }
  foreach ($r in $rules.Preferred) { if ($v -ge $r.Lo -and $v -le $r.Hi) { return 'preferred' } }
  return 'allowed'
}

function Get-VsToolsets([string]$InstallPath) {
  $root = Join-Path $InstallPath 'VC\Tools\MSVC'
  if (-not (Test-Path $root)) { return @() }
  $out = @()
  foreach ($d in Get-ChildItem $root -Directory) {
    $v = $null
    if ([version]::TryParse($d.Name, [ref]$v) -and (Test-Path (Join-Path $d.FullName 'bin\Hostx64\x64\cl.exe'))) {
      $out += [pscustomobject]@{ Version = $v; Verdict = (Test-MsvcAccepted $v) }
    }
  }
  return $out
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
  $t = @($Vs.Toolsets | Sort-Object Version -Descending | ForEach-Object { "$($_.Version) $($_.Verdict)" })
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

  # Components (vswhere -requires answers per instance).
  $vswhere = Get-VsWhere
  $missing = @()
  foreach ($c in $VsRequired) {
    $hit = Get-NativeOutput $vswhere @('-products', '*', '-prerelease', '-path', $vs.Instance.installationPath, '-requires', $c, '-property', 'instanceId')
    if (-not $hit) { $missing += $c }
  }
  $needUpdate = ($vs.Good.Count -eq 0)
  if ($missing.Count -gt 0 -or $needUpdate) {
    $why = @()
    if ($missing.Count -gt 0) { $why += "missing components: $($missing -join ', ')" }
    if ($needUpdate) { $why += "no MSVC toolset the engine accepts (found: $(Format-Toolsets $vs))" }
    if ($script:DoctorOnly) { Fail "$($vs.Name): $($why -join '; ')" 'deepfield setup updates and modifies it.'; return }
    $installer = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
    $path = $vs.Instance.installationPath
    if ($needUpdate) {
      Write-Fix "updating $($vs.Name) to the latest release (brings the current MSVC toolset); approve the UAC prompt"
      $p = Start-Process $installer -Wait -PassThru -ArgumentList @('update', '--installPath', "`"$path`"", '--passive', '--norestart')
      Write-Info "Visual Studio Installer exit code $($p.ExitCode)"
    }
    # Only ids every supported Visual Studio knows: the workloads and what was found missing.
    $addArgs = @()
    foreach ($c in $VsWorkloads + $missing) { $addArgs += @('--add', $c) }
    if (@(Get-WindowsSdks | Where-Object { $_ -ge $WinSdkMinimum }).Count -eq 0 -and $vs.Major -eq 17) { $addArgs += @('--add', 'Microsoft.VisualStudio.Component.Windows11SDK.22621') }
    Write-Fix "adding the C++ game workloads to $($vs.Name); approve the UAC prompt"
    $p = Start-Process $installer -Wait -PassThru -ArgumentList (@('modify', '--installPath', "`"$path`"", '--passive', '--norestart', '--includeRecommended') + $addArgs)
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
      Fail 'Visual Studio still has no MSVC toolset the engine accepts' "Open the Visual Studio Installer > Modify > Individual components, tick an 'MSVC ... x64/x86 build tools' entry whose version is in the`npreferred list above (or at least not refused), and 'Windows 11 SDK (10.0.22621.0)'. Then run this again.`nPlease also send the lines above to whoever maintains this script."
      return
    }
  }
  Write-Ok "$($vs.Name) at $($vs.Instance.installationPath)"
  $top = $vs.Good[0]
  Write-Ok "MSVC $($top.Version) ($($top.Verdict); rules: $(if ($script:Engine) { 'the engine''s Windows_SDK.json' } else { 'built-in minimum until the engine is installed' }))"
  $banned = @($vs.Toolsets | Where-Object { $_.Verdict -eq 'banned' })
  if ($banned.Count -gt 0) { Write-Info "also present, ignored by UBT: $((@($banned | ForEach-Object { $_.Version.ToString() }) -join ', '))" }

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
    Write-Host '        Keep the default location. In Options, "Editor symbols for debugging" is only for C++ debugging' -ForegroundColor Yellow
    Write-Host '        (adds ~60 GB); the rest of the defaults are right.' -ForegroundColor Yellow
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
  if ($branch) { Write-Ok "branch $branch" }

  if (-not $script:DoctorOnly) {
    Invoke-Native 'git' @('-C', $repo, 'config', 'core.longpaths', 'true') | Out-Null
  }

  # Line endings. The JSON content is hashed in the join handshake, so a CRLF checkout disagrees
  # with every other machine about the same commit (Build/windows-bringup.md 3).
  $crlf = Get-NativeOutput 'git' @('-C', $repo, 'config', '--get', 'core.autocrlf')
  $probe = Join-Path $repo 'unreal\Build\test.sh'
  $hasCr = (Test-Path $probe) -and ((Get-Content $probe -Raw) -match "`r`n")
  if (($crlf -and $crlf -ne 'false') -or $hasCr) {
    if ($script:DoctorOnly) { Fail 'the clone converts line endings (core.autocrlf is not false)' 'deepfield setup fixes it when the working tree has no uncommitted changes.'; }
    else {
      $dirty = Get-NativeOutput 'git' @('-C', $repo, 'status', '--porcelain')
      if ($dirty) {
        Write-Warn2 'the clone converts line endings, and there are uncommitted changes, so it is left alone.'
        Write-Info "Commit or stash them, then: git -C `"$repo`" config core.autocrlf false; git -C `"$repo`" reset --hard HEAD"
      } else {
        Write-Fix 'turning off line-ending conversion for this clone and re-checking the files out as committed'
        Invoke-Native 'git' @('-C', $repo, 'config', 'core.autocrlf', 'false') | Out-Null
        Invoke-Native 'git' @('-C', $repo, 'reset', '--hard', '-q', 'HEAD') | Out-Null
        Write-Ok 'line endings as committed'
      }
    }
  } else { Write-Ok 'line endings as committed (core.autocrlf=false)' }

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

function Invoke-Build {
  Write-Step 'Build: DeepFieldEditor Win64 Development'
  $p = Get-Paths
  $logDir = Join-Path $p.Saved 'Logs'; New-Item -ItemType Directory -Force -Path $logDir | Out-Null
  $log = Join-Path $logDir 'build-editor.log'
  Write-Info 'The first build compiles every module and takes 10-30 minutes; later builds only what changed.'
  Write-Info "Full log: $log"
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  try {
    & $p.BuildBat 'DeepFieldEditor' 'Win64' 'Development' "-Project=$($script:Project)" '-WaitMutex' '-NoHotReload' 2>&1 |
      ForEach-Object { "$_" } | Tee-Object -FilePath $log | ForEach-Object {
        if ($_ -match ': (fatal )?error |error [A-Z]+\d+|Result: Failed|BUILD FAILED') { Write-Host $_ -ForegroundColor Red }
        elseif ($_ -match '^\[\d+/\d+\]|Result: Succeeded|Total execution time|Building |Using ') { Write-Host $_ }
      }
    $rc = $LASTEXITCODE
  } finally { $ErrorActionPreference = $old }
  if ($rc -ne 0) {
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
  $newer = @(Get-ChildItem (Join-Path $src 'Source'), (Join-Path $src 'Config') -Recurse -File -Include *.cpp, *.h, *.inl, *.cs, *.ini -ErrorAction SilentlyContinue |
      Where-Object { $_.LastWriteTime -gt $newestBin } | Select-Object -First 1)
  if ($newer.Count -gt 0) {
    $rel = $newer[0].FullName.Substring($script:Repo.Length + 1)
    if ($AllowStale) { Write-Warn2 "$rel is newer than the built modules; testing anyway (-AllowStale)" }
    else { Fail "$rel is newer than the built modules, so the tests would run code that is not in them" 'Build first (deepfield test builds unless the build fails), or pass -AllowStale if you mean it.' }
  }
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
    if ($tests.Count -eq 0) { Fail "no test matched '$Filter'" "A typo in the filter? Log: $log"; return $false }
    if ($failed -gt 0) { Fail "$failed of $($tests.Count) tests failed" "Log: $log"; return $false }
    Write-Ok "$($tests.Count) passed"
    return $true
  }
  if (Test-Path $log) {
    $none = Select-String -Path $log -Pattern 'No automation tests matched' | Select-Object -First 1
    if ($none) { Fail "no test matched '$Filter'" "A typo in the filter? Log: $log"; return $false }
    Select-String -Path $log -Pattern 'Error:|Fatal|Assertion' | Select-Object -First 10 | ForEach-Object { Write-Host "   $($_.Line)" -ForegroundColor Red }
  }
  Fail "the editor exited ($($proc.ExitCode)) without a test report" "Log: $log"
  return $false
}

function Invoke-RepoChecks {
  Write-Step 'Repository checks (no engine needed)'
  $build = Join-Path $script:Repo 'unreal\Build'
  $failed = @()
  foreach ($c in @(@('layering-check.py'), @('validate-content-json.py'), @('check-test-coverage.py'))) {
    Write-Host "   -- $($c -join ' ')"
    $rc = Invoke-Python (@((Join-Path $build $c[0])) + @($c | Select-Object -Skip 1))
    if ($rc -ne 0) { $failed += $c[0] }
  }
  if ($failed.Count -gt 0) { Fail "failed: $($failed -join ', ')" 'The output above says what is wrong.' ; return }
  Write-Ok 'repository checks passed'
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
    if ($Command -eq 'check') {
      $script:Python = Find-Python
      if (-not $script:Python) { Fail 'Python is not installed' 'Run: deepfield setup' }
    } else {
      Read-EngineAssociation
      $script:Engine = Find-Engine $script:EngineAssociation
      if (-not $script:Engine) { Fail "Unreal Engine $($script:EngineAssociation) is not installed" 'Run: deepfield setup' }
      if (@('build', 'test', 'editor', 'play', 'host', 'join') -contains $Command -and -not (Get-VsVerdict | Where-Object { $_.Good.Count -gt 0 })) {
        Fail 'Visual Studio with an MSVC toolset UE 5.8 accepts is not installed' 'Run: deepfield setup'
      }
    }
  }

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
    'check'    { Invoke-RepoChecks }
    'editor'   {
      if (Invoke-Build) {
        $p = Get-Paths
        Start-Process $p.Editor -ArgumentList @("`"$($script:Project)`"") | Out-Null
        Write-Ok 'editor starting (the first open compiles shaders: slow once, fast afterwards)'
      }
    }
    'play'     { if (Invoke-Build) { Start-Game @($Map) 'play' } }
    'host'     {
      if (Invoke-Build) {
        Start-Game @("$Map`?listen", "-port=$Port") 'host'
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
