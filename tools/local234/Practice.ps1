param([Parameter(Mandatory)][string]$Trial,[ValidateSet('UAlbertaBot','Stardust','BananaBrain','Locutus','HladHammer','Steamhammer','Odin','EggBot','Venator','Orion','LocalZealotRush','LocalFourGateDragoon','LocalDTRush')][string]$Opponent='UAlbertaBot')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$trialDir=[IO.Path]::GetFullPath($Trial)
$m=Get-Content (Join-Path $trialDir 'manifest.json') -Raw | ConvertFrom-Json
if($m.phase -ne 'agent' -or [DateTimeOffset]::UtcNow -ge [DateTimeOffset]$m.deadline_utc){throw 'Trial closed'}
$id=(Get-Content (Join-Path $repo "runs/local234-20261026/roster-candidate.json") -Raw | ConvertFrom-Json) | Where-Object name -eq $Opponent
$practice=Join-Path $trialDir 'practice';New-Item -ItemType Directory $practice -Force | Out-Null
$match=Join-Path $practice ('{0:d3}-{1}' -f @(Get-ChildItem $practice -Directory).Count,$Opponent)
$remaining=[Math]::Max(1,[Math]::Floor(([DateTimeOffset]$m.deadline_utc-[DateTimeOffset]::UtcNow).TotalSeconds))
& (Join-Path $PSScriptRoot 'Match.exe') (Join-Path $trialDir 'bin') $id.runtime $id.kind $id.entry '(2)Heartbreak Ridge.scx' 20261026 1 $match ([Math]::Min(600,$remaining)) $id.bwapi
Get-Content (Join-Path $match 'receipt.json')
if(Test-Path (Join-Path $match 'result.json')){Get-Content (Join-Path $match 'result.json');Get-Content (Join-Path $match 'telemetry.csv') -Tail 8}
Write-Output "Evidence: $match"
