# Self-contained launcher for Windows: builds and runs the game with zero PATH
# assumptions. The PowerShell twin of ./play — same flags, same refusals.
#
#   .\play.ps1                                   # windowed
#   .\play.ps1 --headless -- --server            # dedicated server
#   .\play.ps1 -- --shot foundry shot.png        # render a frame
#
# Override the tool locations with $env:DOTNET / $env:GODOT if yours differ
# from the ones docs/INSTALL.md sets up.
$ErrorActionPreference = 'Stop'

$Dotnet = if ($env:DOTNET) { $env:DOTNET } else { Join-Path $env:USERPROFILE '.dotnet\dotnet.exe' }
$Godot = if ($env:GODOT) { $env:GODOT } else { Join-Path $env:USERPROFILE 'Applications\Godot_mono\Godot_v4.7.2-stable_mono_win64.exe' }
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path $Dotnet)) {
    # A machine-wide install (winget / the official installer) puts it on PATH.
    $onPath = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($onPath) { $Dotnet = $onPath.Source } else { Write-Error "missing .NET SDK at $Dotnet - see docs/INSTALL.md"; exit 1 }
}
if (-not (Test-Path $Godot)) {
    $onPath = Get-Command godot -ErrorAction SilentlyContinue
    if ($onPath) { $Godot = $onPath.Source } else { Write-Error "missing Godot at $Godot - see docs/INSTALL.md"; exit 1 }
}

# Godot's mono module locates hostfxr by running `dotnet` and reading
# DOTNET_ROOT - both must point at the same install or the engine boots
# without C# ("Failed to load hostfxr").
$env:DOTNET_ROOT = Split-Path -Parent $Dotnet
$env:PATH = "$env:DOTNET_ROOT;$env:PATH"

# Engine flags after `--` are silently ignored: Godot hands everything past
# the separator to the game as user args. A --quit-after on the wrong side
# means a headless run never exits, and nothing looks wrong. Refuse instead.
$seenSep = $false
foreach ($arg in $args) {
    if ($arg -eq '--') { $seenSep = $true; continue }
    if ($seenSep -and ($arg -match '^--(quit-after(=.*)?|quit|headless|path|import|build-solutions)$')) {
        Write-Error "'$arg' is an engine flag but sits after '--', where Godot ignores it. Move it before the separator:  .\play.ps1 --headless $arg -- --solo <map>"
        exit 2
    }
}

& $Dotnet build (Join-Path $Here 'game\DeepField.Game.csproj') -v quiet
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Godot --path (Join-Path $Here 'game') @args
exit $LASTEXITCODE
