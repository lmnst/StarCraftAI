param()
$ErrorActionPreference='Stop'
$root='D:\xjzb\openbw-readiness-20261027'
$probe=Join-Path $root ('preflight/'+[DateTimeOffset]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory (Join-Path $probe src) -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot Starter.cpp) (Join-Path $probe src/Bot.cpp)
# Exercise the exact .NET timestamp format used by real timed trials, before timing starts.
@{phase='operator_preflight';deadline_utc=[DateTimeOffset]::UtcNow.AddHours(1).ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $probe manifest.json) -Encoding utf8
& (Join-Path $PSScriptRoot Practice.ps1) -Trial $probe -Action Preflight -Games 1 -Opponents @('Locutus')
$summary=Get-Content (Join-Path $probe 'practice/batch-000/summary.json') -Raw -Encoding utf8 | ConvertFrom-Json
if($summary.planned -ne 1 -or $summary.unresolved -ne 0){throw "Preflight match did not pass: $probe"}
$checks=@{}
foreach($name in @('task.md','Starter.cpp','practice.py','Practice.ps1','StartTrial.ps1','Preflight.ps1','session_check.py')){
 $checks[$name]=(Get-FileHash (Join-Path $PSScriptRoot $name)).Hash
}
$record=@{passed=$true;completed_utc=[DateTimeOffset]::UtcNow.ToString('o');directory=$probe;tool_hashes=$checks;environment_sha256=(Get-FileHash (Join-Path $root environment.json)).Hash;summary=$summary;scope='operator tool check, not model training or benchmark performance'}
$record | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $probe preflight.json) -Encoding utf8
Write-Output "Preflight passed: $probe"
# Return the concrete record to StartTrial without mixing it with command output.
return (Join-Path $probe preflight.json)
