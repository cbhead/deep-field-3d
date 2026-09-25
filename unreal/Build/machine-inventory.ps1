# Measures this Windows machine and writes what it found into the repository, so no session has
# to guess what the box has. Read-only: it changes no setting, installs nothing, needs no admin
# (a few answers - Defender exclusions - are only visible elevated and say so).
#
#   powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1
#   powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1 -Name windows-gpu -NoDxDiag
#
# Writes unreal\Build\machines\<Name>.md (read this) and <Name>.json (the raw facts). Every
# requirement it checks is quoted from windows-bringup.md; if that file changes, change the checks
# here in the same PR. Re-run it after installing anything and commit both files.
param(
    [string]$Name = 'windows-gpu',
    [switch]$NoDxDiag
)
$ErrorActionPreference = 'Continue'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $Here '..\..')).Path
$OutDir = Join-Path $Here 'machines'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory $OutDir | Out-Null }

# ---------------------------------------------------------------- helpers

function Try-Get([scriptblock]$Block) { try { & $Block } catch { $null } }

# Runs a native tool and returns its text (stdout + stderr), or $null if it could not start.
function Run([string]$Exe, [string[]]$ArgList) {
    try {
        $out = & $Exe @ArgList 2>&1 | ForEach-Object { "$_" }
        ($out -join "`n").Trim()
    } catch { $null }
}

function RegValue([string]$Path, [string]$Value) {
    Try-Get { (Get-ItemProperty -Path $Path -Name $Value -ErrorAction Stop).$Value }
}

# PATH first, then the places installers put things when they do not touch PATH. A hit off PATH is
# reported as such: a tool a new shell cannot find is half-installed.
function Find-Tool([string]$Command, [string[]]$Candidates) {
    $cmd = Get-Command $Command -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd -and $cmd.Source -notlike '*\WindowsApps\*') { return @{ Path = $cmd.Source; OnPath = $true } }
    foreach ($c in $Candidates) {
        $hit = Get-Item -Path ([Environment]::ExpandEnvironmentVariables($c)) -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($hit) { return @{ Path = $hit.FullName; OnPath = $false } }
    }
    if ($cmd) { return @{ Path = $cmd.Source; OnPath = $true; Stub = $true } }
    $null
}

function GB([double]$Bytes) { [math]::Round($Bytes / 1GB, 1) }
function Val($v) { if ($null -eq $v -or "$v" -eq '') { '-' } else { "$v" } }

# ---------------------------------------------------------------- hardware and OS

$cs   = Get-CimInstance Win32_ComputerSystem
$os   = Get-CimInstance Win32_OperatingSystem
$bios = Get-CimInstance Win32_BIOS
$cpu  = Get-CimInstance Win32_Processor | Select-Object -First 1
$cv   = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'

$system = [ordered]@{
    hostname     = $env:COMPUTERNAME
    manufacturer = $cs.Manufacturer
    model        = $cs.Model
    bios         = "$($bios.SMBIOSBIOSVersion) ($(Try-Get { $bios.ReleaseDate.ToString('yyyy-MM-dd') }))"
}

$osInfo = [ordered]@{
    caption        = $os.Caption
    display_version = RegValue $cv 'DisplayVersion'
    build          = "$($os.BuildNumber).$(RegValue $cv 'UBR')"
    architecture   = $os.OSArchitecture
    installed      = Try-Get { $os.InstallDate.ToString('yyyy-MM-dd') }
    last_boot      = Try-Get { $os.LastBootUpTime.ToString('yyyy-MM-dd HH:mm') }
    latest_hotfix  = Try-Get { (Get-HotFix | Where-Object InstalledOn | Sort-Object InstalledOn -Descending |
                        Select-Object -First 1 | ForEach-Object { "$($_.HotFixID) ($($_.InstalledOn.ToString('yyyy-MM-dd')))" }) }
    powershell     = $PSVersionTable.PSVersion.ToString()
    elevated       = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
                        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

$cpuInfo = [ordered]@{
    name           = $cpu.Name.Trim()
    cores          = $cpu.NumberOfCores
    logical        = $cpu.NumberOfLogicalProcessors
    max_mhz        = $cpu.MaxClockSpeed
    virtualization = if ($cs.HypervisorPresent) { 'on (a hypervisor is running)' } elseif ($cpu.VirtualizationFirmwareEnabled) { 'on' } else { 'off in firmware' }
}

$dimms = @(Get-CimInstance Win32_PhysicalMemory)
$memory = [ordered]@{
    total_gb   = GB $cs.TotalPhysicalMemory
    slots      = Try-Get { (Get-CimInstance Win32_PhysicalMemoryArray | Measure-Object MemoryDevices -Sum).Sum }
    modules    = @($dimms | ForEach-Object {
                    [ordered]@{ slot = $_.DeviceLocator; gb = GB $_.Capacity; mts = $_.ConfiguredClockSpeed
                                part = "$($_.Manufacturer) $("$($_.PartNumber)".Trim())".Trim() } })
    pagefile   = @(Get-CimInstance Win32_PageFileUsage | ForEach-Object { "$($_.Name) $($_.AllocatedBaseSize) MB" })
}

# GPUs: WMI for every adapter (docks show up here too), nvidia-smi for the numbers WMI gets wrong
# (AdapterRAM is a 32-bit field and caps at 4 GB).
$gpus = @(Get-CimInstance Win32_VideoController | ForEach-Object {
    [ordered]@{ name = $_.Name; wmi_driver = $_.DriverVersion; driver_date = Try-Get { $_.DriverDate.ToString('yyyy-MM-dd') }
                resolution = if ($_.CurrentHorizontalResolution) { "$($_.CurrentHorizontalResolution)x$($_.CurrentVerticalResolution) @ $($_.CurrentRefreshRate) Hz" } else { $null } }
})
$nvidia = $null
$smi = Find-Tool 'nvidia-smi' @('C:\Windows\System32\nvidia-smi.exe')
if ($smi) {
    $q = Run $smi.Path @('--query-gpu=name,driver_version,memory.total,compute_cap,pcie.link.gen.max,pcie.link.width.max,vbios_version,power.limit', '--format=csv,noheader')
    $smiHead = Run $smi.Path @()
    if ($q -and $q -notmatch 'failed|error') {
        $f = $q.Split("`n")[0].Split(',') | ForEach-Object { $_.Trim() }
        $nvidia = [ordered]@{
            name = $f[0]; driver = $f[1]; vram = $f[2]; compute_capability = $f[3]
            pcie = "Gen$($f[4]) x$($f[5])"; vbios = $f[6]; power_limit = $f[7]
            cuda = if ($smiHead -match 'CUDA Version:\s*([\d.]+)') { $Matches[1] } else { $null }
        }
    }
}
$graphicsDrivers = 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers'
$gpuSettings = [ordered]@{
    hardware_accelerated_scheduling = switch (RegValue $graphicsDrivers 'HwSchMode') { 2 { 'on' } 1 { 'off' } default { 'default' } }
}

# dxdiag is the one place Windows states the D3D feature levels and WDDM model; it takes ~20 s.
$dx = $null
if (-not $NoDxDiag) {
    $dxFile = Join-Path $env:TEMP "df-dxdiag-$PID.txt"
    Start-Process -FilePath dxdiag.exe -ArgumentList '/t', $dxFile -Wait -WindowStyle Hidden
    if (Test-Path $dxFile) {
        $txt = Get-Content $dxFile -Raw
        Remove-Item $dxFile -ErrorAction SilentlyContinue
        $dx = [ordered]@{ directx = if ($txt -match 'DirectX Version:\s*(.+)') { $Matches[1].Trim() } }
        # One block per display device; keep the NVIDIA one.
        $blocks = $txt -split '(?m)^\s*Card name:'
        $nv = $blocks | Where-Object { $_ -match '^\s*NVIDIA' } | Select-Object -First 1
        if ($nv) {
            foreach ($k in 'Feature Levels', 'Driver Model', 'Hybrid Graphics GPU', 'Dedicated Memory', 'Shared Memory') {
                if ($nv -match "(?m)^\s*$([regex]::Escape($k)):\s*(.+)$") { $dx[$k.ToLower().Replace(' ', '_')] = $Matches[1].Trim() }
            }
        }
    }
}

# A disk with no lettered volume is usually one Windows cannot read (APFS/HFS+ from the Mac).
$disks = @(Try-Get { Get-PhysicalDisk | ForEach-Object {
    $letters = @(Try-Get { Get-Partition -DiskNumber $_.DeviceId -ErrorAction Stop | Where-Object DriveLetter | ForEach-Object { "$($_.DriveLetter):" } })
    [ordered]@{ name = $_.FriendlyName; media = "$($_.MediaType)"; bus = "$($_.BusType)"; size_gb = GB $_.Size; health = "$($_.HealthStatus)"
                drives = if ($letters) { $letters -join ' ' } else { 'no volume Windows can mount' } } } })
$volumes = @(Get-Volume | Where-Object { $_.DriveLetter } | Sort-Object DriveLetter | ForEach-Object {
    [ordered]@{ drive = "$($_.DriveLetter):"; type = "$($_.DriveType)"; label = $_.FileSystemLabel; fs = $_.FileSystem; size_gb = GB $_.Size; free_gb = GB $_.SizeRemaining } })

$network = @(Try-Get { Get-NetAdapter -Physical | Where-Object Status -eq 'Up' | ForEach-Object {
    [ordered]@{ name = $_.Name; adapter = $_.InterfaceDescription; link = $_.LinkSpeed } } })

$power = Try-Get { ((powercfg /getactivescheme) -replace '^.*\(|\)\s*$', '').Trim() }

# ---------------------------------------------------------------- Windows settings the runbook touches

$defender = [ordered]@{}
$mp = Try-Get { Get-MpComputerStatus -ErrorAction Stop }
if ($mp) { $defender.realtime = $mp.RealTimeProtectionEnabled; $defender.antivirus = $mp.AMServiceEnabled }
$pref = Try-Get { Get-MpPreference -ErrorAction Stop }
if ($pref) {
    $ex = @($pref.ExclusionPath) + @($pref.ExclusionProcess) | Where-Object { $_ }
    $defender.exclusions = if ($ex -match 'Must be an administrator') { 'unknown (run elevated to read)' } else { @($ex) }
}
$settings = [ordered]@{
    long_paths     = (RegValue 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' 'LongPathsEnabled') -eq 1
    developer_mode = (RegValue 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' 'AllowDevelopmentWithoutDevLicense') -eq 1
    game_mode      = (RegValue 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled') -ne 0
    defender       = $defender
}

# ---------------------------------------------------------------- toolchain

$pf = $env:ProgramFiles; $pf86 = ${env:ProgramFiles(x86)}; $lad = $env:LOCALAPPDATA
$toolSpecs = [ordered]@{
    git       = @('git',     @("$pf\Git\cmd\git.exe"),                                 @('--version'))
    git_lfs   = @('git-lfs', @("$pf\Git\mingw64\bin\git-lfs.exe", "$pf\Git LFS\git-lfs.exe"), @('version'))
    gh        = @('gh',      @("$pf\GitHub CLI\gh.exe"),                               @('--version'))
    python    = @('python',  @("$lad\Programs\Python\Python3*\python.exe", "$pf\Python3*\python.exe", 'C:\Windows\py.exe'), @('--version'))
    dotnet    = @('dotnet',  @("$pf\dotnet\dotnet.exe", "$env:USERPROFILE\.dotnet\dotnet.exe"), @('--version'))
    node      = @('node',    @("$pf\nodejs\node.exe"),                                 @('--version'))
    cmake     = @('cmake',   @("$pf\CMake\bin\cmake.exe"),                             @('--version'))
    clang_cl  = @('clang-cl',@("$pf\LLVM\bin\clang-cl.exe"),                           @('--version'))
    blender   = @('blender', @("$pf\Blender Foundation\Blender*\blender.exe"),         @('--version'))
    claude    = @('claude',  @("$env:USERPROFILE\.local\bin\claude.exe", "$env:APPDATA\npm\claude.cmd"), @('--version'))
    vscode    = @('code',    @("$pf\Microsoft VS Code\bin\code.cmd", "$lad\Programs\Microsoft VS Code\bin\code.cmd"), @('--version'))
    '7zip'    = @('7z',      @("$pf\7-Zip\7z.exe"),                                    @())
}
$tools = [ordered]@{}
foreach ($k in $toolSpecs.Keys) {
    $s = $toolSpecs[$k]
    $t = Find-Tool $s[0] $s[1]
    if (-not $t) { $tools[$k] = [ordered]@{ found = $false }; continue }
    $ver = if ($t.Stub) { 'Microsoft Store alias only (not installed)' } else { (Run $t.Path $s[2]) -split "`n" | Select-Object -First 1 }
    $tools[$k] = [ordered]@{ found = -not $t.Stub; on_path = $t.OnPath; path = $t.Path; version = $ver }
}
# dotnet --version fails outright when only runtimes are installed; list both instead.
$dotnetSdks = @()
if ($tools.dotnet.found) {
    $dotnetSdks = @((Run $tools.dotnet.path @('--list-sdks')) -split "`n" | Where-Object { $_ -match '^\d' } | ForEach-Object { ($_ -split ' ')[0] })
    $tools.dotnet.runtimes = @((Run $tools.dotnet.path @('--list-runtimes')) -split "`n" | Where-Object { $_ -match '^Microsoft\.NETCore\.App ' } | ForEach-Object { ($_ -split ' ')[1] })
    $tools.dotnet.version = if ($dotnetSdks) { "SDK $($dotnetSdks -join ', ')" } else { "runtimes only (no SDK): $($tools.dotnet.runtimes -join ', ')" }
}
$tools.dotnet.sdks = $dotnetSdks

$gitCfg = [ordered]@{}
if ($tools.git.found) {
    foreach ($k in 'core.longpaths', 'core.autocrlf', 'filter.lfs.process', 'user.name') {
        $v = Run $tools.git.path @('config', '--get', $k); $gitCfg[$k] = if ($LASTEXITCODE -eq 0) { $v } else { $null }
    }
    $v = Run $tools.git.path @('-C', $Repo, 'config', '--local', '--get', 'lfs.fetchexclude')
    $gitCfg['lfs.fetchexclude (this clone)'] = if ($LASTEXITCODE -eq 0) { $v } else { $null }
}
if ($tools.gh.found) {
    $gitCfg['gh auth'] = if ((Run $tools.gh.path @('auth', 'status')) -match 'Logged in to github.com') { 'logged in' } else { 'not logged in' }
}

# Visual Studio, via the installer's own vswhere.
$vswhere = "$pf86\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = @()
if (Test-Path $vswhere) {
    $json = Run $vswhere @('-all', '-prerelease', '-products', '*', '-format', 'json', '-utf8')
    foreach ($i in @(Try-Get { $json | ConvertFrom-Json })) {
        $msvcDir = Join-Path $i.installationPath 'VC\Tools\MSVC'
        $vs += [ordered]@{
            name     = $i.displayName
            version  = $i.installationVersion
            path     = $i.installationPath
            msvc     = @(Get-ChildItem $msvcDir -Directory -ErrorAction SilentlyContinue | ForEach-Object Name)
        }
    }
}
$winSdks = @(Get-ChildItem "$pf86\Windows Kits\10\Include" -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'um\windows.h') } | ForEach-Object Name)

# Unreal: the launcher's install list, the engine's registry keys, and the usual folder.
$ue = @()
$roots = @()
$launcherDat = 'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
if (Test-Path $launcherDat) {
    $roots += @(Try-Get { (Get-Content $launcherDat -Raw | ConvertFrom-Json).InstallationList |
        Where-Object { $_.AppName -like 'UE_*' } | ForEach-Object InstallLocation })
}
$roots += @(Get-ChildItem 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' -ErrorAction SilentlyContinue |
    ForEach-Object { RegValue $_.PSPath 'InstalledDirectory' })
$roots += @(Get-ChildItem "$pf\Epic Games" -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
if ($env:UE_ROOT) { $roots += $env:UE_ROOT }
foreach ($r in ($roots | Where-Object { $_ } | ForEach-Object { $_.TrimEnd('\') } | Sort-Object -Unique)) {
    $bv = Try-Get { Get-Content (Join-Path $r 'Engine\Build\Build.version') -Raw | ConvertFrom-Json }
    $sdkJson = Try-Get { Get-Content (Join-Path $r 'Engine\Config\Windows\Windows_SDK.json') -Raw | ConvertFrom-Json }
    $ue += [ordered]@{
        root       = $r
        version    = if ($bv) { "$($bv.MajorVersion).$($bv.MinorVersion).$($bv.PatchVersion) (CL $($bv.Changelist))" } else { 'no Build.version' }
        editor     = Test-Path (Join-Path $r 'Engine\Binaries\Win64\UnrealEditor.exe')
        windows_sdk_json = $sdkJson
    }
}
$epic = [ordered]@{
    launcher = Test-Path "$pf86\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
    engines  = $ue
}

$envVars = [ordered]@{}
foreach ($v in 'UE_ROOT', 'UE-LocalDataCachePath', 'UE_LOCAL_DDC', 'EDITOR_LOCK_DIR') {
    $envVars[$v] = [Environment]::GetEnvironmentVariable($v, 'Machine'), [Environment]::GetEnvironmentVariable($v, 'User') |
        Where-Object { $_ } | Select-Object -First 1
}

# ---------------------------------------------------------------- this clone and the CI runner

$git = $tools.git.path
$clone = [ordered]@{
    path        = $Repo
    path_length = $Repo.Length
    branch      = if ($git) { Run $git @('-C', $Repo, 'rev-parse', '--abbrev-ref', 'HEAD') } else { $null }
    head        = if ($git) { Run $git @('-C', $Repo, 'log', '-1', '--format=%h %cs %s') } else { $null }
    worktrees   = if ($git) { @((Run $git @('-C', $Repo, 'worktree', 'list')) -split "`n").Count } else { $null }
    ddc_sibling = Test-Path (Join-Path $Repo '..\DDC')
}
$runner = [ordered]@{
    services = @(Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Name) ($($_.Status))" })
    folders  = @('C:\actions-runner', 'D:\actions-runner') | Where-Object { Test-Path "$_\config.cmd" }
}

# ---------------------------------------------------------------- verdicts against windows-bringup.md

function VerParts([string]$v) { @($v.Split('.') | ForEach-Object { [int]$_ }) }
function MsvcVerdict([string]$v) {
    $p = VerParts $v; $mm = "$($p[0]).$($p[1])"; $b = $p[2]
    if ($mm -eq '14.44' -and $b -ge 35211) { return 'preferred' }
    if ($mm -eq '14.50' -and $b -ge 35723) { return 'preferred' }
    if ($mm -eq '14.39' -or ($p[0] -eq 14 -and $p[1] -ge 40 -and $p[1] -le 43) -or $mm -eq '14.44' -or $mm -eq '14.50') { return 'banned' }
    if ($p[0] -eq 14 -and ($p[1] -gt 38 -or ($p[1] -eq 38 -and $b -ge 33130))) { return 'allowed' }
    'too old'
}

$checks = New-Object System.Collections.ArrayList
function Check([string]$Item, [string]$Need, [string]$Have, [string]$Status, [string]$Where) {
    [void]$checks.Add([ordered]@{ item = $Item; need = $Need; have = $Have; status = $Status; runbook = $Where })
}

$build = [int]$os.BuildNumber
Check 'Windows' 'Windows 11, or 10 >= 19041' "$($osInfo.caption) build $($osInfo.build)" $(if ($build -ge 19041) { 'OK' } else { 'FAIL' }) '0'

$nvName = if ($nvidia) { "$($nvidia.name), driver $($nvidia.driver), $($nvidia.vram)" } else { 'no NVIDIA GPU found' }
Check 'NVIDIA GPU + current driver' 'current Studio / Game Ready driver' $nvName $(if ($nvidia) { 'OK' } else { 'FAIL' }) '0'

$best = $volumes | Where-Object { $_.type -eq 'Fixed' } | Sort-Object { $_.free_gb } -Descending | Select-Object -First 1
$bestTxt = if ($best) { "$($best.drive) $($best.free_gb) GB free of $($best.size_gb) GB" } else { 'no fixed volume' }
Check 'Free NVMe space' '>= 500 GB free on one volume' $bestTxt $(if ($best -and $best.free_gb -ge 500) { 'OK' } else { 'FAIL' }) '0'

Check 'Short clone root' 'D:\DF\deepfield-3d (short path)' "$($clone.path) ($($clone.path_length) chars)" $(if ($clone.path_length -le 24) { 'OK' } else { 'WARN' }) '0, 3'

Check 'Long paths enabled' 'LongPathsEnabled = 1' "$($settings.long_paths)" $(if ($settings.long_paths) { 'OK' } else { 'FAIL' }) '0'

$exTxt = if ($defender.exclusions -is [string]) { $defender.exclusions } elseif ($defender.exclusions) { ($defender.exclusions -join '; ') } else { 'none' }
$exStatus = if ($defender.exclusions -is [string]) { 'UNKNOWN' } elseif ($defender.exclusions) { 'OK' } else { 'FAIL' }
Check 'Defender exclusions' 'clone, engine, UnrealEditor/cl/link/... processes' $exTxt $exStatus '0'

$ue58 = $ue | Where-Object { $_.version -like '5.8.2*' } | Select-Object -First 1
$ueTxt = if ($ue) { ($ue | ForEach-Object { "$($_.version) at $($_.root)" }) -join '; ' } else { 'none installed' }
Check 'Unreal Engine 5.8.2' 'UE 5.8.2 from the launcher (same patch as the Mac)' $ueTxt $(if ($ue58) { 'OK' } else { 'FAIL' }) '1'
Check 'UE_ROOT' 'set to the 5.8 install' $(if ($envVars.UE_ROOT) { $envVars.UE_ROOT } else { 'unset' }) $(if ($envVars.UE_ROOT -and (Test-Path $envVars.UE_ROOT)) { 'OK' } else { 'FAIL' }) '1'

$allMsvc = @($vs | ForEach-Object { $_.msvc }) | Where-Object { $_ }
$msvcTxt = if ($allMsvc) { ($allMsvc | ForEach-Object { "$_ ($(MsvcVerdict $_))" }) -join '; ' } else { 'no Visual Studio / MSVC' }
$msvcStatus = if ($allMsvc | Where-Object { (MsvcVerdict $_) -eq 'preferred' }) { 'OK' } elseif ($allMsvc | Where-Object { (MsvcVerdict $_) -eq 'allowed' }) { 'WARN' } else { 'FAIL' }
Check 'MSVC toolset' '14.44 >= 35211 (VS 2022 17.14) or 14.50 >= 35723' $msvcTxt $msvcStatus '2'

$sdkTxt = if ($winSdks) { $winSdks -join ', ' } else { 'none' }
$sdkStatus = if ($winSdks -contains '10.0.22621.0') { 'OK' } elseif ($winSdks | Where-Object { [version]$_ -ge [version]'10.0.19041.0' }) { 'WARN' } else { 'FAIL' }
Check 'Windows SDK' '10.0.22621.0 (10.0.19041.0 minimum)' $sdkTxt $sdkStatus '2'

$dn8 = $dotnetSdks | Where-Object { $_ -like '8.*' }
Check '.NET 8 SDK' 'for tools/content-export, waveplan-golden' $(if ($dotnetSdks) { $dotnetSdks -join ', ' } else { 'no SDK' }) $(if ($dn8) { 'OK' } else { 'FAIL' }) '2'

$gitTxt = if ($tools.git.found) { "$($tools.git.version)$(if (-not $tools.git.on_path) { ' - NOT on PATH' })" } else { 'not installed' }
Check 'Git for Windows' 'installed, on PATH' $gitTxt $(if ($tools.git.found -and $tools.git.on_path) { 'OK' } elseif ($tools.git.found) { 'WARN' } else { 'FAIL' }) '3'
Check 'Git LFS' 'git lfs install done' "$($tools.git_lfs.version); filter.lfs.process=$($gitCfg['filter.lfs.process'])" $(if ($gitCfg['filter.lfs.process']) { 'OK' } else { 'FAIL' }) '3'
Check 'core.longpaths' 'true' (Val $gitCfg['core.longpaths']) $(if ($gitCfg['core.longpaths'] -eq 'true') { 'OK' } else { 'FAIL' }) '3'
Check 'core.autocrlf' 'false' "$($gitCfg['core.autocrlf'])" $(if ($gitCfg['core.autocrlf'] -eq 'false') { 'OK' } else { 'FAIL' }) '3'
$lfsEx = $gitCfg['lfs.fetchexclude (this clone)']
Check 'LFS light-clone undone' 'lfs.fetchexclude = "" in this clone' $(if ($null -eq $lfsEx) { 'unset (the .lfsconfig Mac excludes apply)' } else { "'$lfsEx'" }) $(if ($null -ne $lfsEx -and $lfsEx -eq '') { 'OK' } else { 'FAIL' }) '3'
Check 'GitHub CLI' 'installed, gh auth login' $(if ($tools.gh.found) { "$($tools.gh.version); $($gitCfg['gh auth'])" } else { 'not installed' }) $(if ($gitCfg['gh auth'] -eq 'logged in') { 'OK' } else { 'FAIL' }) '3'

$pyOk = $tools.python.found -and ($tools.python.version -match 'Python 3\.(\d+)') -and ([int]$Matches[1] -ge 9)
Check 'Python 3.9+' 'for the unreal\Build\*.py checks' "$($tools.python.version)" $(if ($pyOk) { 'OK' } else { 'FAIL' }) '5'

$runTxt = if ($runner.services -or $runner.folders) { (@($runner.services) + @($runner.folders)) -join '; ' } else { 'none' }
Check 'Self-hosted runner' 'deepfield-gpu, labels deepfield,gpu' $runTxt $(if ($runner.services -or $runner.folders) { 'OK' } else { 'FAIL' }) '8'

# ---------------------------------------------------------------- write

$when = Get-Date
$facts = [ordered]@{
    generated = $when.ToString('yyyy-MM-ddTHH:mm:sszzz'); generator = 'unreal/Build/machine-inventory.ps1'
    system = $system; os = $osInfo; cpu = $cpuInfo; memory = $memory
    gpus = $gpus; nvidia = $nvidia; dxdiag = $dx; gpu_settings = $gpuSettings
    disks = $disks; volumes = $volumes; network = $network; power_plan = $power
    settings = $settings; tools = $tools; git = $gitCfg; visual_studio = $vs; windows_sdks = $winSdks
    epic = $epic; env = $envVars; clone = $clone; runner = $runner; checks = $checks
}

function Row([object[]]$Cells) { '| ' + (($Cells | ForEach-Object { "$_".Replace('|', '\|').Replace("`n", ' ') }) -join ' | ') + ' |' }

$md = New-Object System.Collections.Generic.List[string]
$failCount = @($checks | Where-Object { $_.status -in 'FAIL', 'UNKNOWN' }).Count
$md.Add("# $Name - machine inventory")
$md.Add('')
$md.Add("_Generated $($when.ToString('yyyy-MM-dd HH:mm zzz')) on ``$($system.hostname)`` by ``unreal/Build/machine-inventory.ps1``._")
$md.Add('_Do not edit by hand - re-run the script (`powershell -ExecutionPolicy Bypass -File unreal\Build\machine-inventory.ps1`) and commit both this file and the `.json` beside it._')
$md.Add('')
$md.Add("This is what the box **measurably has** at that instant, and how far it is from ``windows-bringup.md``. ")
$md.Add("If you are about to rely on something about this machine, check here first; if it is older than your last install, re-run the script. " +
        "$($checks.Count) bring-up checks, $failCount not met or not knowable without elevation.")
$md.Add('')
$md.Add('## Bring-up readiness')
$md.Add('')
$md.Add((Row 'Status', 'Item', 'Runbook needs', 'This machine has', 'Runbook section'))
$md.Add('|---|---|---|---|---|')
foreach ($c in $checks) { $md.Add((Row $c.status, $c.item, $c.need, $c.have, $c.runbook)) }
$md.Add('')
$md.Add('`OK` meets the runbook; `WARN` works but is not what the runbook asks for; `FAIL` missing or wrong; `UNKNOWN` needs an elevated run to read.')
$md.Add('')

$md.Add('## Hardware')
$md.Add('')
$md.Add('| | |')
$md.Add('|---|---|')
$md.Add((Row 'Machine', "$($system.manufacturer) $($system.model) (BIOS $($system.bios))"))
$md.Add((Row 'CPU', "$($cpuInfo.name) - $($cpuInfo.cores) cores / $($cpuInfo.logical) threads, max $($cpuInfo.max_mhz) MHz, virtualization $($cpuInfo.virtualization)"))
$dimmTxt = ($memory.modules | ForEach-Object { "$($_.slot) $($_.gb) GB @ $($_.mts) MT/s ($($_.part))" }) -join ', '
$md.Add((Row 'Memory', "$($memory.total_gb) GB usable; $(@($memory.modules).Count) of $(Val $memory.slots) slots filled: $dimmTxt"))
$md.Add((Row 'Page file', (Val ($memory.pagefile -join ', '))))
if ($nvidia) {
    $md.Add((Row 'GPU', "$($nvidia.name), $($nvidia.vram), compute $($nvidia.compute_capability), PCIe $($nvidia.pcie), power limit $($nvidia.power_limit)"))
    $md.Add((Row 'NVIDIA driver', "$($nvidia.driver) (CUDA $(Val $nvidia.cuda), VBIOS $($nvidia.vbios))"))
}
if ($dx) {
    $md.Add((Row 'DirectX', "$(Val $dx.directx); feature levels $(Val $dx.feature_levels); $(Val $dx.driver_model)"))
}
$md.Add((Row 'HW GPU scheduling', $gpuSettings.hardware_accelerated_scheduling))
foreach ($g in $gpus) { $md.Add((Row 'Display adapter', "$($g.name) - driver $($g.wmi_driver) ($(Val $g.driver_date))$(if ($g.resolution) { ", $($g.resolution)" })")) }
foreach ($d in $disks) { $md.Add((Row 'Disk', "$($d.name) - $($d.bus) $($d.media), $($d.size_gb) GB, $($d.health), $($d.drives)")) }
foreach ($v in $volumes) { $md.Add((Row 'Volume', "$($v.drive) $(if ($v.label) { "'$($v.label)' " })$($v.type) $($v.fs), $($v.free_gb) GB free of $($v.size_gb) GB")) }
foreach ($n in $network) { $md.Add((Row 'Network', "$($n.name): $($n.adapter), $($n.link)")) }
$md.Add((Row 'Power plan', (Val $power)))
$md.Add('')

$md.Add('## Operating system and settings')
$md.Add('')
$md.Add('| | |')
$md.Add('|---|---|')
$md.Add((Row 'Windows', "$($osInfo.caption) $($osInfo.display_version), build $($osInfo.build), $($osInfo.architecture)"))
$md.Add((Row 'Installed / last boot', "$(Val $osInfo.installed) / $(Val $osInfo.last_boot)"))
$md.Add((Row 'Latest update', (Val $osInfo.latest_hotfix)))
$md.Add((Row 'Windows PowerShell', $osInfo.powershell))
$md.Add((Row 'Long paths', $settings.long_paths))
$md.Add((Row 'Developer mode', $settings.developer_mode))
$md.Add((Row 'Game mode', $settings.game_mode))
$md.Add((Row 'Defender real-time', (Val $defender.realtime)))
$md.Add((Row 'Inventory ran elevated', $osInfo.elevated))
$md.Add('')

$md.Add('## Toolchain')
$md.Add('')
$md.Add((Row 'Tool', 'Version', 'On PATH', 'Path'))
$md.Add('|---|---|---|---|')
foreach ($k in $tools.Keys) {
    $t = $tools[$k]
    if ($t.found) { $md.Add((Row $k, (Val $t.version), $(if ($t.on_path) { 'yes' } else { '**no**' }), $t.path)) }
    elseif ($t.version) { $md.Add((Row $k, $t.version, '-', $t.path)) }
    else { $md.Add((Row $k, 'not installed', '-', '-')) }
}
$md.Add('')
$md.Add((Row '.NET SDKs', (Val ($dotnetSdks -join ', '))))
$md.Add('|---|---|')
$md.Add((Row 'Visual Studio', $(if ($vs) { ($vs | ForEach-Object { "$($_.name) $($_.version) - MSVC $(Val ($_.msvc -join ', '))" }) -join '; ' } else { 'not installed' })))
$md.Add((Row 'Windows SDKs', (Val ($winSdks -join ', '))))
$md.Add((Row 'Epic Games Launcher', $(if ($epic.launcher) { 'installed' } else { 'not installed' })))
$md.Add((Row 'Unreal Engine', $ueTxt))
foreach ($k in $envVars.Keys) { $md.Add((Row "env $k", (Val $envVars[$k]))) }
foreach ($k in $gitCfg.Keys) { $md.Add((Row "git $k", (Val $gitCfg[$k]))) }
$md.Add('')

$md.Add('## This clone and CI')
$md.Add('')
$md.Add('| | |')
$md.Add('|---|---|')
$md.Add((Row 'Clone', "$($clone.path) ($($clone.path_length) chars)"))
$md.Add((Row 'Branch / HEAD at inventory', "$(Val $clone.branch) / $(Val $clone.head)"))
$md.Add((Row 'Worktrees', (Val $clone.worktrees)))
$md.Add((Row 'Sibling DDC folder', $clone.ddc_sibling))
$md.Add((Row 'Actions runner', $runTxt))
$md.Add('')

$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $OutDir "$Name.md"), (($md -join "`n") + "`n"), $utf8)
[IO.File]::WriteAllText((Join-Path $OutDir "$Name.json"), (($facts | ConvertTo-Json -Depth 8) -replace "`r`n", "`n") + "`n", $utf8)

Write-Host "wrote unreal\Build\machines\$Name.md and .json"
foreach ($c in $checks) { Write-Host ("{0,-8} {1}" -f $c.status, $c.item) }
