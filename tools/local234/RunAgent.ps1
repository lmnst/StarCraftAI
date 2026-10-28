param([string]$Name='agent-001',[int]$BudgetMinutes=60)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root=Join-Path $repo 'runs/local234-20261026'
if($Name -ne 'agent-001'){throw 'Automatic review is authorized only for independent trial agent-001'}
$trial=Join-Path $root $Name
if(Test-Path $trial){throw 'Use a new independent trial directory'}
New-Item -ItemType Directory (Join-Path $trial src) -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot Miner.cpp) (Join-Path $trial src/Bot.cpp)
Copy-Item (Join-Path $trial src) (Join-Path $trial initial) -Recurse
$task=(Get-Content (Join-Path $repo benchmark/local234-20261026/task.md) -Raw).Replace('REPO',$repo).Replace('tools/local234/Practice.ps1','tools/local234/RequestPractice.ps1')
[IO.File]::WriteAllText((Join-Path $trial task.md),$task)
& (Join-Path $PSScriptRoot Build.ps1) -Trial $trial
New-Item -ItemType Directory (Join-Path $trial requests) -Force | Out-Null
$start=[DateTimeOffset]::UtcNow;$deadline=$start.AddMinutes($BudgetMinutes)
$manifest=[ordered]@{kind='single independent local CLI bot trial, NOT official';phase='agent';repo=$repo;model='gpt-6.1-sol';reasoning_effort='medium';cli_version=(& codex --version | Select-Object -Last 1);budget_minutes=$BudgetMinutes;started_utc=$start.ToString('o');deadline_utc=$deadline.ToString('o');initial_source_sha256=(Get-FileHash (Join-Path $trial src/Bot.cpp)).Hash;prompt_sha256=(Get-FileHash (Join-Path $trial task.md)).Hash;isolation='web_search disabled, ignore user config, sandbox network_access=false, workspace-write with user-authorized automatic review for agent-001 only; read isolation and allowed file edits cooperative; operator broker runs practice outside sandbox';authorization='User explicitly approved --approve-for-me for this one trial and authorized quota-based settings; fixed Sol / medium for the full 60 minutes'; segments=@()}
$manifestPath=Join-Path $trial manifest.json
function SaveManifest{$manifest | ConvertTo-Json -Depth 12 | Set-Content $manifestPath}
SaveManifest
$codexExe=(Get-Command codex).Source;$cli=$null;$worker=$null;$seen=@{};$segment=0;$session=$null
try{
 while([DateTimeOffset]::UtcNow -lt $deadline){
  if(!$cli -or $cli.HasExited){
   if($cli){[void]$copyOut.GetAwaiter().GetResult();[void]$copyErr.GetAwaiter().GetResult();$outFile.Dispose();$errFile.Dispose();$manifest.segments[-1].exit_code=$cli.ExitCode;SaveManifest;$events=Get-Content (Join-Path $trial "events-$($segment-1).jsonl") | ForEach-Object {try{$_ | ConvertFrom-Json}catch{}};$session=($events | Where-Object type -eq 'thread.started' | Select-Object -First 1).thread_id;if(!$session -and !$manifest.segments[-1].session){throw 'CLI failed before an independent session started'};if(!$session){$session=$manifest.segments[-1].session}}
   $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=$codexExe;$psi.WorkingDirectory=$trial;$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardInput=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true;$psi.StandardInputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardOutputEncoding=[Text.UTF8Encoding]::new($false);$psi.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
   $arguments=@('exec','--json','--ignore-user-config','--skip-git-repo-check','--approve-for-me','-m','gpt-6.1-sol','-c','model_reasoning_effort="medium"','-c','web_search="disabled"','-c','sandbox_workspace_write.network_access=false','-c','features.multi_agent=false')
   if($session){$arguments+=@('resume',$session,'-')}else{$arguments+=@('-C',$trial,'-')}
   foreach($a in $arguments){$psi.ArgumentList.Add($a)}
   $eventsPath=Join-Path $trial "events-$segment.jsonl";$stderrPath=Join-Path $trial "stderr-$segment.log"
   $cli=[Diagnostics.Process]::Start($psi)
   $outFile=[IO.FileStream]::new($eventsPath,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read);$errFile=[IO.FileStream]::new($stderrPath,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
   $copyOut=$cli.StandardOutput.BaseStream.CopyToAsync($outFile);$copyErr=$cli.StandardError.BaseStream.CopyToAsync($errFile)
   $prompt=if($session){"Continue the same timed trial. Deadline UTC: $($deadline.ToString('o')). Read your prior notes and practice feedback. Use remaining time for improvement and keep src/Bot.cpp compiling. No held-out evaluation has run."}else{$task+"`nTrial directory: $trial`nDeadline UTC: $($deadline.ToString('o'))"}
   $cli.StandardInput.WriteLine($prompt);$cli.StandardInput.Close()
   $manifest.segments+=@([ordered]@{index=$segment;pid=$cli.Id;started_utc=[DateTimeOffset]::UtcNow.ToString('o');session=$session;exit_code=$null;arguments=$arguments});SaveManifest;$segment++
  }
  if($worker -and $worker.HasExited){$worker=$null}
  if(!$worker){
   $request=Get-ChildItem (Join-Path $trial requests) -Filter '*.request.json' -ErrorAction SilentlyContinue | Where-Object {!$seen.ContainsKey($_.FullName)} | Sort-Object LastWriteTime | Select-Object -First 1
   if($request){$seen[$request.FullName]=$true;$worker=Start-Process pwsh -ArgumentList @('-NoProfile','-File',('"'+(Join-Path $PSScriptRoot PracticeWorker.ps1)+'"'),'-Trial',('"'+$trial+'"'),'-Request',('"'+$request.FullName+'"')) -WindowStyle Hidden -PassThru}
  }

  Start-Sleep -Milliseconds 250
 }
}finally{
 foreach($p in @($cli,$worker)){if($p -and !$p.HasExited){$p.Kill($true);$p.WaitForExit()}}
 if($outFile){try{[void]$copyOut.GetAwaiter().GetResult();$outFile.Dispose();[void]$copyErr.GetAwaiter().GetResult();$errFile.Dispose()}catch{}}
 $frozen=Join-Path $trial 'frozen';New-Item -ItemType Directory $frozen -Force | Out-Null
 Copy-Item (Join-Path $trial src) (Join-Path $frozen src) -Recurse
 Copy-Item (Join-Path $trial bin) (Join-Path $frozen agent-bin) -Recurse
 $manifest.phase='frozen';$manifest.frozen_utc=[DateTimeOffset]::UtcNow.ToString('o');$manifest.final_source_sha256=(Get-FileHash (Join-Path $frozen src/Bot.cpp)).Hash;SaveManifest
}
# Compile exactly the collected source, outside the agent budget, without edits.
& (Join-Path $PSScriptRoot Build.ps1) -Trial (Join-Path $trial frozen)
Get-FileHash (Join-Path $trial frozen/bin/Candidate.exe) | Select-Object Hash | ConvertTo-Json | Set-Content (Join-Path $trial frozen/binary.json)
Write-Output "Frozen final bot: $trial"
