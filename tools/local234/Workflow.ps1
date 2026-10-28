$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $repo 'runs/local234-20261026'
$statusPath=Join-Path $run status.json
$state=Get-Content $statusPath -Raw | ConvertFrom-Json
$state.phase='preparing_authorized_independent_trial'
$state.auto_review_script_change='Explicit user approval received; scoped to agent-001 only'
$state | Add-Member -NotePropertyName workflow_pid -NotePropertyValue $PID -Force
$state | Add-Member -NotePropertyName agent_settings -NotePropertyValue @{model='gpt-6.1-sol';reasoning_effort='medium';budget_minutes=60;reason='User authorized quota-based adjustment; next five-hour reset occurs during the trial'} -Force
$state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
$state | ConvertTo-Json -Depth 12 | Set-Content $statusPath
try{
 & (Join-Path $PSScriptRoot RunAgent.ps1) -Name agent-001 -BudgetMinutes 60
 $manifest=Get-Content (Join-Path $run agent-001/manifest.json) -Raw | ConvertFrom-Json
 if($manifest.phase -ne 'frozen'){throw 'Independent trial did not freeze'}
 $frozen=Join-Path $run agent-001/frozen
 if(!(Test-Path (Join-Path $frozen bin/Candidate.exe))){throw 'Collected source did not produce a final binary'}
 $state.phase='evaluating_frozen_agent'
 $state.independent_timed_agent_started=$true
 $state | Add-Member -NotePropertyName candidate -NotePropertyValue @{directory=$frozen;source_sha256=(Get-FileHash (Join-Path $frozen src/Bot.cpp)).Hash;binary_sha256=(Get-FileHash (Join-Path $frozen bin/Candidate.exe)).Hash;trial_manifest='agent-001/manifest.json'} -Force
 $state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
 $state | ConvertTo-Json -Depth 12 | Set-Content $statusPath
 & (Join-Path $PSScriptRoot Evaluate.ps1) -Frozen $frozen -OutputName evaluation-001
 $summary=Get-Content (Join-Path $run evaluation-001/summary.json) -Raw | ConvertFrom-Json
 $state.evaluation_completed=$summary.completed;$state.score=$summary.score
 $state.phase=if($summary.completed -eq 234){'completed_local_234'}else{'partial_evaluation'}
 $state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
 $state | ConvertTo-Json -Depth 12 | Set-Content $statusPath
 $summary | ConvertTo-Json -Depth 12
}catch{
 $state.phase='workflow_error'
 $state | Add-Member -NotePropertyName error -NotePropertyValue $_.Exception.Message -Force
 $state.updated_utc=[DateTimeOffset]::UtcNow.ToString('o')
 $state | ConvertTo-Json -Depth 12 | Set-Content $statusPath
 throw
}
