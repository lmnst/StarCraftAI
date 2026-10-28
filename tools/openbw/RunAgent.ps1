param([ValidateSet('agent-002')][string]$Name='agent-002',[string]$Model='gpt-6.1-sol',[ValidateSet('low','medium')][string]$Reasoning='medium',[switch]$ApprovedForThisTrial)
$ErrorActionPreference='Stop'
if(!$ApprovedForThisTrial){throw 'This new trial needs its own authorization for --approve-for-me; the old agent-001 authorization does not apply.'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root=Join-Path $repo 'runs/openbw-20261027'
$accept=Get-Content (Join-Path $root acceptance.json) -Raw | ConvertFrom-Json
if(!$accept.passed){throw 'OpenBW acceptance must pass before starting the independent timer'}
if(!$accept.all_13_fixtures_checked){throw 'Only practice-tool acceptance has run; the future independent trial is not qualified yet'}
if(!$accept.official_scoring_ready){throw 'The public OpenBW kill+razing score backend is not implemented; timed benchmark is not ready'}
if((Get-FileHash (Join-Path $root environment.json)).Hash.ToLower() -ne $accept.environment_sha256){throw 'Environment changed after acceptance'}
$trial=Join-Path $root $Name
if(Test-Path $trial){throw 'Independent trial directory already exists; do not overwrite or silently restart'}
New-Item -ItemType Directory (Join-Path $trial src) -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot Miner.cpp) (Join-Path $trial src/Bot.cpp)
Copy-Item (Join-Path $trial src) (Join-Path $trial initial) -Recurse
$task=(Get-Content (Join-Path $repo benchmark/openbw-20261027/task.md) -Raw).Replace('REPO_PATH',$repo)
[IO.File]::WriteAllText((Join-Path $trial task.md),$task,[Text.UTF8Encoding]::new($false))
Copy-Item (Join-Path $root roster.json) (Join-Path $trial roster.json)
Copy-Item (Join-Path $root quota-before-trial.json) (Join-Path $trial quota-before-trial.json)
$start=[DateTimeOffset]::UtcNow;$deadline=$start.AddMinutes(60)
$manifest=[ordered]@{scope='single independent local OpenBW trial; NOT official';phase='agent';model=$Model;reasoning_effort=$Reasoning;budget_minutes=60;started_utc=$start.ToString('o');deadline_utc=$deadline.ToString('o');initial_source_sha256=(Get-FileHash (Join-Path $trial src/Bot.cpp)).Hash;prompt_sha256=(Get-FileHash (Join-Path $trial task.md)).Hash;acceptance_sha256=(Get-FileHash (Join-Path $root acceptance.json)).Hash;authorization='This invocation only; no global settings changed';isolation='Web disabled; Windows shell network disabled; workspace-write with automatic approval review. Opponent/source read restrictions cooperative. Practice is a fixed one-shot WSL command, no resident broker or scheduled task.';segments=@()}
$manifestPath=Join-Path $trial manifest.json
function SaveManifest{$manifest | ConvertTo-Json -Depth 12 | Set-Content $manifestPath -Encoding utf8}
SaveManifest
$codexExe=(Get-Command codex).Source;$cli=$null;$session=$null;$segment=0
try{
 while([DateTimeOffset]::UtcNow -lt $deadline){
  if(!$cli -or $cli.HasExited){
   if($cli){
    [void]$copyOut.GetAwaiter().GetResult();[void]$copyErr.GetAwaiter().GetResult();$outFile.Dispose();$errFile.Dispose()
    $manifest.segments[-1].exit_code=$cli.ExitCode;SaveManifest
    $events=Get-Content (Join-Path $trial "events-$($segment-1).jsonl") | ForEach-Object {try{$_ | ConvertFrom-Json}catch{}}
    if($events | Where-Object {($_.type -eq 'turn.failed') -or ($_.type -eq 'error')}){throw 'CLI reported a failed turn; trial cannot be reported as an uninterrupted full hour. Inspect events.'}
    $newSession=($events | Where-Object type -eq 'thread.started' | Select-Object -First 1).thread_id
    if($newSession){$session=$newSession}
    if(!$session){throw 'CLI ended before a valid independent session started'}
   }
   $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=$codexExe;$psi.WorkingDirectory=$trial;$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardInput=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
   $psi.StandardInputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardOutputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
   $arguments=@('exec','--json','--ignore-user-config','--skip-git-repo-check','--approve-for-me','-m',$Model,'-c',('model_reasoning_effort="'+$Reasoning+'"'),'-c','web_search="disabled"','-c','sandbox_workspace_write.network_access=false','-c','features.multi_agent=false')
   if($session){$arguments+=@('resume',$session,'-')}else{$arguments+=@('-C',$trial,'-')}
   foreach($a in $arguments){$psi.ArgumentList.Add($a)}
   $cli=[Diagnostics.Process]::Start($psi)
   $outFile=[IO.FileStream]::new((Join-Path $trial "events-$segment.jsonl"),[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
   $errFile=[IO.FileStream]::new((Join-Path $trial "stderr-$segment.log"),[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
   $copyOut=$cli.StandardOutput.BaseStream.CopyToAsync($outFile);$copyErr=$cli.StandardError.BaseStream.CopyToAsync($errFile)
   $prompt=if($session){"Continue the same independent timed trial until $($deadline.ToString('o')). Use the remaining time to improve from your practice summaries; no held-out evaluation has run. Do not change model, rules, or operator tools."}else{$task+"`nTrial directory: $trial`nHard UTC deadline: $($deadline.ToString('o'))"}
   $cli.StandardInput.WriteLine($prompt);$cli.StandardInput.Close()
   $manifest.segments+=@([ordered]@{index=$segment;pid=$cli.Id;started_utc=[DateTimeOffset]::UtcNow.ToString('o');session=$session;exit_code=$null;arguments=$arguments});SaveManifest;$segment++
  }
  Start-Sleep -Milliseconds 250
 }
 $manifest.phase='frozen_after_full_hour'
}catch{
 $manifest.phase='interrupted_trial';$manifest.error=$_.Exception.Message
 throw
}finally{
 if($cli -and !$cli.HasExited){$cli.Kill($true);$cli.WaitForExit()}
 if($outFile){try{[void]$copyOut.GetAwaiter().GetResult();[void]$copyErr.GetAwaiter().GetResult();$outFile.Dispose();$errFile.Dispose()}catch{}}
 $frozen=Join-Path $trial frozen;New-Item -ItemType Directory $frozen -Force | Out-Null
 Copy-Item (Join-Path $trial src) (Join-Path $frozen src) -Recurse
 $manifest.frozen_utc=[DateTimeOffset]::UtcNow.ToString('o');$manifest.final_source_sha256=(Get-FileHash (Join-Path $frozen src/Bot.cpp)).Hash;SaveManifest
}
# Compile collected source without edits. Full evaluation is a separate one-shot command.
& (Join-Path $PSScriptRoot Practice.ps1) -Trial $frozen -Action Build
Write-Output "Frozen independent source and binary: $frozen"
