param(
    [string]$GameSource,
    [string]$Replay,
    [ValidateRange(8, 80)][int]$Speed = 16
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $GameSource) {
    $GameSource = (Get-Content -LiteralPath "$root/game-source.txt" -Raw).Trim()
}
$run = Join-Path $root ("runs/" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$map = 'maps/BroodWar/AIIDE/(2)Heartbreak Ridge.scx'
$module = if ($Replay) { 'ReplayAudit.dll' } else { 'Candidate.dll' }
$dll = Join-Path $root "build/Release/$module"
$originalMap = $map
$saveReplay = 'maps/replays/command-demo-$H$M$S.rep'
if ($Replay) {
    $Replay = (Resolve-Path -LiteralPath $Replay).Path
    $map = 'maps/replays/input.rep'
    $saveReplay = ''
}
foreach ($file in @("$GameSource/StarCraft.exe", "$GameSource/injectory.x86.exe", "$GameSource/bwapi-data/BWAPI.dll", "$GameSource/$originalMap", $dll, "$root/ddraw.dll")) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing required file: $file" }
}
New-Item -ItemType Directory -Path "$run/bwapi-data/AI","$run/bwapi-data/read","$run/bwapi-data/write","$run/maps/BroodWar/AIIDE","$run/maps/replays","$run/characters","$run/tmp" | Out-Null
Get-ChildItem -LiteralPath $GameSource -File | Where-Object {
    $_.Extension -in '.dll','.mpq','.snp' -and $_.Name -ne 'ddraw.dll'
} | Copy-Item -Destination $run
foreach ($name in 'StarCraft.exe','injectory.x86.exe') {
    Copy-Item -LiteralPath "$GameSource/$name" -Destination $run
}
Copy-Item -LiteralPath "$GameSource/bwapi-data/BWAPI.dll" -Destination "$run/bwapi-data"
if ($Replay) { Copy-Item -LiteralPath $Replay -Destination "$run/$map" }
else { Copy-Item -LiteralPath "$GameSource/$map" -Destination "$run/$map" }
Copy-Item -LiteralPath $dll -Destination "$run/bwapi-data/AI/$module"
Copy-Item -LiteralPath "$root/ddraw.dll" -Destination $run
@"
[ddraw]
windowed=true
fullscreen=false
width=960
height=720
renderer=gdi
savesettings=0
[StarCraft]
game_handles_close=true
"@ | Set-Content -LiteralPath "$run/ddraw.ini" -Encoding ascii
@"
[ai]
ai=bwapi-data/AI/$module
ai_dbg=NULL
[auto_menu]
auto_menu=SINGLE_PLAYER
character_name=StarBench
auto_restart=OFF
map=$map
race=Protoss
enemy_count=1
enemy_race=Protoss
game_type=MELEE
save_replay=$saveReplay
[config]
shared_memory=OFF
console_attach_auto=FALSE
console_alloc_auto=FALSE
[window]
windowed=ON
left=20
top=40
width=640
height=480
[starcraft]
sound=OFF
speed_override=$Speed
"@ | Set-Content -LiteralPath "$run/bwapi-data/bwapi.ini" -Encoding ascii

Write-Output "Run: $run"
$names = 'BWAPI_CONFIG_INI','STARCRAFT_COMMAND_MODE','TEMP','TMP'
$old = @{}
foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    $env:BWAPI_CONFIG_INI = "$run/bwapi-data/bwapi.ini"
    $env:STARCRAFT_COMMAND_MODE = 'demo'
    $env:TEMP = "$run/tmp"
    $env:TMP = "$run/tmp"
    $proc = Start-Process -FilePath "$run/injectory.x86.exe" -ArgumentList '--launch StarCraft.exe --inject bwapi-data\BWAPI.dll --wait-for-exit' -WorkingDirectory $run -PassThru -WindowStyle Normal -RedirectStandardOutput "$run/launcher.log" -RedirectStandardError "$run/launcher-error.log"
    Set-Content -LiteralPath "$run/launcher.pid" -Value $proc.Id -Encoding ascii
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
}

