$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $repo 'runs/local234-20261026'
$output=Join-Path $run evaluation-001
$statePath=Join-Path $run status.json
$state=Get-Content $statePath -Raw | ConvertFrom-Json
$identity=Get-Content (Join-Path $output candidate.json) -Raw | ConvertFrom-Json
$frozen=Join-Path $run agent-001/frozen
if((Get-FileHash (Join-Path $frozen bin/Candidate.exe)).Hash -ne $identity.binary_sha256 -or
   (Get-FileHash (Join-Path $frozen src/Bot.cpp)).Hash -ne $identity.source_sha256){throw 'Frozen candidate changed'}
foreach($entry in @(Get-Content (Join-Path $run protocol-sha256.json) -Raw | ConvertFrom-Json)){
 if((Get-FileHash $entry.Path).Hash -ne $entry.Hash){throw "Protocol or schedule changed: $($entry.Path)"}
}
$stamp=Get-Date -Format yyyyMMdd-HHmmss
$existing=@(Get-ChildItem $output -File -Filter 'm*.result.json' | Sort-Object Name)
$receipt=[ordered]@{kind='User-authorized continuation of the same fixed evaluation';requested_by_user='继续';resumed_utc=[DateTimeOffset]::UtcNow.ToString('o');pid=$PID;completed_before=$existing.Count;binary_sha256=$identity.binary_sha256;source_sha256=$identity.source_sha256;existing_results=@($existing | ForEach-Object {@{file=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}})}
$receipt | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run "resume-$stamp.json")
$state.phase='evaluating_frozen_agent';$state.workflow_pid=$PID
$state.PSObject.Properties.Remove('error')
$state.PSObject.Properties.Remove('pause_reason')
$state | Add-Member -NotePropertyName resumed_utc -NotePropertyValue $receipt.resumed_utc -Force
$state.updated_utc=$receipt.resumed_utc
$state | ConvertTo-Json -Depth 12 | Set-Content $statePath
try{
 & (Join-Path $PSScriptRoot Evaluate.ps1) -Frozen $frozen -OutputName evaluation-001
 $summary=Get-Content (Join-Path $output summary.json) -Raw | ConvertFrom-Json
 foreach($entry in $receipt.existing_results){if((Get-FileHash (Join-Path $output $entry.file)).Hash -ne $entry.sha256){throw "Existing result changed: $($entry.file)"}}
 $state.evaluation_completed=$summary.completed;$state.score=$summary.score
 $state.phase=if($summary.completed -eq 234){'completed_local_234'}else{'partial_evaluation'}
 $state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
 $state | ConvertTo-Json -Depth 12 | Set-Content $statePath
 $summary | ConvertTo-Json -Depth 12
}catch{
 $state.phase='evaluation_error'
 $state | Add-Member -NotePropertyName error -NotePropertyValue $_.Exception.Message -Force
 $state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
 $state | ConvertTo-Json -Depth 12 | Set-Content $statePath
 throw
}
