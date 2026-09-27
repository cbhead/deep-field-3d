<#
.SYNOPSIS
  Record the game playing itself: a solo match on a map in attract mode (-DFDemo), every frame dumped
  at a fixed step, encoded to an MP4.

.DESCRIPTION
  Starts the game (the editor binary with -game, or a packaged DeepField.exe with -Exe) on the map in
  endless mode (?endless, so the waves keep coming and grow until they break through; -NoEndless plays
  the authored arc) with -DFDemo -DFDemoCapture -benchmark -fps=<Fps>. -benchmark steps the world a
  fixed 1/Fps per frame however long a frame takes to render, and -DFDemoCapture has ADFDemoDirector
  take a screenshot with the HUD every frame (-dumpmovie would leave the UI out), so the video plays at
  real speed even when capture is slow. ADFDemoDirector builds towers from the economy and films from
  an orbit (DFMatch/Public/Dev/DFDemoDirector.h). When Seconds x Fps frames exist, the game is closed
  (only the process this script started) and ffmpeg encodes them.

  ffmpeg comes from the imageio-ffmpeg Python package: python -m pip install --user imageio-ffmpeg

.EXAMPLE
  powershell -File unreal\Build\record-demo.ps1 -Seconds 150 -Out C:\Users\me\Videos\deepfield-demo.mp4
#>
param(
  [string]$Map = '/Game/DF/Maps/Testlane/L_Testlane',
  [int]$Seconds = 150,
  [int]$Fps = 20,
  [int]$ResX = 1280,
  [int]$ResY = 720,
  [string]$Out = '',
  # A packaged DeepField.exe; default: the engine's UnrealEditor.exe -game on this clone's project.
  [string]$Exe = '',
  # Play the authored waves only (the default is ?endless).
  [switch]$NoEndless,
  # Extra game arguments.
  [string]$ExtraArgs = ''
)
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Join-Path $repo 'unreal\DeepField\DeepField.uproject'
if (-not $Out) { $Out = Join-Path $repo ("unreal\DeepField\Saved\Demo\deepfield-demo-{0:yyyyMMdd-HHmm}.mp4" -f (Get-Date)) }
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null

$ffmpeg = (& python -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())").Trim()
if (-not (Test-Path $ffmpeg)) { throw 'ffmpeg not found: python -m pip install --user imageio-ffmpeg' }

$url = if ($NoEndless) { $Map } else { "$Map`?endless" }
if ($Exe) {
  $gameDir = Split-Path $Exe
  $shots = Join-Path $gameDir 'DeepField\Saved\Screenshots\Windows'
  $file = $Exe
  $args0 = @($url)
} else {
  $ueRoot = $env:UE_ROOT
  if (-not $ueRoot) { $ueRoot = 'C:\Program Files\Epic Games\UE_5.8' }
  $file = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
  $shots = Join-Path $repo 'unreal\DeepField\Saved\Screenshots\WindowsEditor'
  $args0 = @("`"$project`"", $url, '-game')
}
if (Test-Path $shots) { Remove-Item -Recurse -Force $shots }

$gameArgs = $args0 + @('-windowed', "-ResX=$ResX", "-ResY=$ResY", '-DFDemo', '-DFDemoCapture', '-benchmark', "-fps=$Fps",
  '-nosplash', '-NoVerifyGC', '-log', "-ABSLOG=`"$(Join-Path (Split-Path $Out) 'record-demo.log')`"")
if ($ExtraArgs) { $gameArgs += $ExtraArgs }
$frames = $Seconds * $Fps
Write-Host "record-demo: $url, $Seconds s at $Fps fps ($frames frames), ${ResX}x$ResY -> $Out"
$proc = Start-Process -FilePath $file -ArgumentList $gameArgs -PassThru
try {
  $deadline = (Get-Date).AddSeconds([Math]::Max(600, $Seconds * 20))
  $last = -1
  while ($true) {
    Start-Sleep -Seconds 5
    $n = if (Test-Path $shots) { @(Get-ChildItem $shots -Filter '*.png' -Recurse).Count } else { 0 }
    if ($n -ne $last) { Write-Host "  $n / $frames frames"; $last = $n }
    if ($n -ge $frames) { break }
    if ($proc.HasExited) { throw "the game exited early (code $($proc.ExitCode)); see the log beside $Out" }
    if ((Get-Date) -gt $deadline) { throw "only $n of $frames frames before the deadline" }
  }
} finally {
  # A packaged DeepField.exe is a bootstrap that starts Binaries\Win64\DeepField.exe as its child:
  # stop the children this run started first, then the process itself (nothing else is touched).
  Get-CimInstance Win32_Process -Filter "ParentProcessId=$($proc.Id)" -ErrorAction SilentlyContinue |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
  if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}

# Frames in the order they were written (their names are sequential; sort by name to be sure).
$list = Join-Path (Split-Path $Out) 'record-demo-frames.txt'
$pngs = Get-ChildItem $shots -Filter '*.png' -Recurse | Sort-Object Name | Select-Object -First $frames
$pngs | ForEach-Object { "file '$($_.FullName -replace '\\', '/')'`nduration $(1.0 / $Fps)" } | Set-Content -Encoding ascii $list
& $ffmpeg -y -loglevel error -f concat -safe 0 -i $list -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p" -r $Fps -c:v libx264 -crf 22 -preset medium $Out
if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed ($LASTEXITCODE)" }
Write-Host "record-demo: wrote $Out ($([Math]::Round((Get-Item $Out).Length / 1MB, 1)) MB)"
