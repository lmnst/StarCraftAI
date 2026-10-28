param([Parameter(Mandatory)][string]$Name,[string]$Model='gpt-5.6-sol',[string]$Reasoning='medium',[ValidateRange(1,60)][int]$BudgetMinutes=60)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$'){throw 'Use a simple unique trial name'}
. (Join-Path $PSScriptRoot '../LocalPaths.ps1')
$paths=Get-LocalPaths
$root=$paths.runtime_root
$config=Get-Content (Join-Path $root environment.json) -Raw -Encoding utf8 | ConvertFrom-Json
if(!$config.ready -or !$config.scoring_ready){throw 'Windows roster/scoring acceptance has not passed'}
$acceptancePath=Join-Path $root 'pipeline-acceptance/acceptance.json'
if(!(Test-Path -LiteralPath $acceptancePath)){throw 'Final pipeline acceptance record is missing'}
$acceptance=Get-Content -LiteralPath $acceptancePath -Raw -Encoding utf8 | ConvertFrom-Json
if(!$acceptance.passed -or $acceptance.environment_sha256 -ne (Get-FileHash (Join-Path $root environment.json)).Hash){throw 'Final acceptance does not match the configured environment'}
$trial=Join-Path $root "trials/$Name"
if(Test-Path -LiteralPath $trial){throw 'Trial directory already exists; choose a new name'}
$preflightOutput=@(& (Join-Path $PSScriptRoot Preflight.ps1))
$preflightPath=[string]$preflightOutput[-1]
$preflight=Get-Content -LiteralPath $preflightPath -Raw -Encoding utf8 | ConvertFrom-Json
if(!$preflight.passed){throw 'Preflight did not pass; training was not started'}
New-Item -ItemType Directory (Join-Path $trial src) -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot Starter.cpp) (Join-Path $trial src/Bot.cpp)
Copy-Item (Join-Path $trial src) (Join-Path $trial initial) -Recurse
$task=(Get-Content (Join-Path $PSScriptRoot task.md) -Raw -Encoding utf8).Replace('TOOLS_PATH',$PSScriptRoot).Replace('TRIAL_PATH',$trial).Replace('BWAPI_INCLUDE_PATH',$paths.bwapi_include).Replace('SDK_PATH',(Join-Path $root sdk))
[IO.File]::WriteAllText((Join-Path $trial task.md),$task,[Text.UTF8Encoding]::new($false))
$config.opponents | Select-Object name,race,official_version_match | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $trial roster.json) -Encoding utf8
$started=[DateTimeOffset]::UtcNow;$deadline=$started.AddMinutes($BudgetMinutes)
$manifest=[ordered]@{scope='local Windows OpenBW; ten versions including explicit tscmoo author candidate; NOT official';phase='training';model=$Model;reasoning=$Reasoning;budget_minutes=$BudgetMinutes;started_utc=$started.ToString('o');deadline_utc=$deadline.ToString('o');environment_sha256=(Get-FileHash (Join-Path $root environment.json)).Hash;initial_source_sha256=(Get-FileHash (Join-Path $trial src/Bot.cpp)).Hash;prompt_sha256=(Get-FileHash (Join-Path $trial task.md)).Hash;isolation='workspace-write with automatic approval review; web and shell network disabled; opponent/source read restrictions cooperative';segments=@()}
function SaveManifest{$manifest | ConvertTo-Json -Depth 14 | Set-Content (Join-Path $trial manifest.json) -Encoding utf8}
SaveManifest
$manifest.preflight=$preflightPath
$manifest.effective_context_verified=$false
SaveManifest
$cli=$null;$outFile=$null;$errFile=$null;$session=$null;$segment=0;$failure=$null
try{
 while([DateTimeOffset]::UtcNow -lt $deadline){
  if(!$cli -or $cli.HasExited){
   if($cli){
    [void]$copyOut.GetAwaiter().GetResult();[void]$copyErr.GetAwaiter().GetResult();$outFile.Dispose();$errFile.Dispose();$outFile=$null;$errFile=$null
    $manifest.segments[-1].exit_code=$cli.ExitCode;SaveManifest
    $events=Get-Content (Join-Path $trial "events-$($segment-1).jsonl") | ForEach-Object {try{$_ | ConvertFrom-Json}catch{}}
    if($cli.ExitCode -ne 0 -or ($events | Where-Object {$_.type -in @('error','turn.failed')})){throw 'Training CLI failed; inspect the saved events before resuming'}
    $newSession=($events | Where-Object type -eq 'thread.started' | Select-Object -First 1).thread_id
    if($newSession){$session=$newSession}
    if(!$session){throw 'CLI ended without an independent session ID'}
   }
   $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=(Get-Command codex).Source;$psi.WorkingDirectory=$trial;$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardInput=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
   $psi.StandardInputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardOutputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
   $arguments=@('exec','--json','--ignore-user-config','--skip-git-repo-check','--approve-for-me','-c','default_permissions=":workspace"','-m',$Model,'-c',('model_reasoning_effort="'+$Reasoning+'"'),'-c','web_search="disabled"','-c','features.multi_agent=false')
   if($session){$arguments+=@('resume',$session,'-')}else{$arguments+=@('-C',$trial,'-')}
   foreach($arg in $arguments){$psi.ArgumentList.Add($arg)}
   $cli=[Diagnostics.Process]::Start($psi)
   # The checker tails these files while the CLI runs; disable FileStream buffering.
   $outFile=[IO.FileStream]::new((Join-Path $trial "events-$segment.jsonl"),[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read,1)
   $errFile=[IO.FileStream]::new((Join-Path $trial "stderr-$segment.log"),[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read,1)
   $copyOut=$cli.StandardOutput.BaseStream.CopyToAsync($outFile);$copyErr=$cli.StandardError.BaseStream.CopyToAsync($errFile)
   $prompt=if($session){"Continue the SAME local timed trial until UTC $($deadline.ToString('o')). Your objective remains maximizing match win rate with a complete playable bot. The initial mining code is only a starting point, never a restriction on construction or combat. Reassess strategy using practice losses; do not optimize mining metrics instead of winning. No held-out test has run. Keep src compiling; do not read opponents or change tools or settings."}else{$task+"`nHard UTC deadline: $($deadline.ToString('o'))"}
   $cli.StandardInput.WriteLine($prompt);$cli.StandardInput.Close()
   $manifest.segments+=@([ordered]@{index=$segment;pid=$cli.Id;session=$session;started_utc=[DateTimeOffset]::UtcNow.ToString('o');exit_code=$null});SaveManifest;$segment++
   $manifest.effective_context_verified=$false;$contextStarted=[DateTimeOffset]::UtcNow;SaveManifest
  }
  if(!$manifest.effective_context_verified){
   & $paths.python (Join-Path $PSScriptRoot session_check.py) --trial $trial --model $Model --reasoning $Reasoning --min-turns $segment
   if($LASTEXITCODE -eq 0){$manifest.effective_context_verified=$true;SaveManifest}
   elseif($LASTEXITCODE -ne 3){throw 'Actual training permissions/model did not match the requested settings'}
   elseif(([DateTimeOffset]::UtcNow-$contextStarted).TotalSeconds -gt 60){throw 'Actual training context could not be verified within 60 seconds'}
  }
  Start-Sleep -Milliseconds 250
 }
 $manifest.phase='frozen_after_full_budget'
}catch{$failure=$_.Exception.Message;$manifest.phase='interrupted_trial';$manifest.error=$failure
}finally{
 if($cli -and !$cli.HasExited){$cli.Kill($true);$cli.WaitForExit()}
 if($outFile){try{[void]$copyOut.GetAwaiter().GetResult();[void]$copyErr.GetAwaiter().GetResult()}catch{};$outFile.Dispose();$errFile.Dispose()}
 $frozen=Join-Path $trial frozen;New-Item -ItemType Directory $frozen -Force | Out-Null
 Copy-Item (Join-Path $trial src) (Join-Path $frozen src) -Recurse
 $manifest.frozen_utc=[DateTimeOffset]::UtcNow.ToString('o');SaveManifest
}
if($failure){throw $failure}
try{
 & (Join-Path $PSScriptRoot Practice.ps1) -Trial $frozen -Action Build
 @{source_files=@(Get-ChildItem (Join-Path $frozen src) -File | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}});binary_sha256=(Get-FileHash (Join-Path $frozen Candidate.dll)).Hash.ToLower();frozen_utc=$manifest.frozen_utc;environment_sha256=$manifest.environment_sha256} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $frozen frozen.json) -Encoding utf8
 $manifest.phase='evaluation';SaveManifest
 & (Join-Path $PSScriptRoot Practice.ps1) -Trial $frozen -Action Evaluate
 $summaryPath=Join-Path $frozen 'evaluation/summary.json'
 $summary=Get-Content -Raw -LiteralPath $summaryPath -Encoding utf8 | ConvertFrom-Json
 $manifest.evaluation_summary=$summaryPath
 if($summary.planned -ne 180 -or $summary.unresolved -ne 0){throw 'Evaluation has unresolved or missing scheduled games; inspect the saved summary'}
 $manifest.phase='completed';$manifest.completed_utc=[DateTimeOffset]::UtcNow.ToString('o');SaveManifest
 Write-Output "Completed local training and 180 resolved test games: $frozen"
}catch{
 $manifest.phase='evaluation_failed';$manifest.error=$_.Exception.Message;SaveManifest
 throw
}
