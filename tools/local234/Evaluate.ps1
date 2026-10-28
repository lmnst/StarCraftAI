param([Parameter(Mandatory)][string]$Frozen,[string]$OutputName='evaluation-001',[int]$Limit=234)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $repo 'runs/local234-20261026'
$protocol=Get-Content (Join-Path $run protocol.json) -Raw | ConvertFrom-Json
$schedule=@(Get-Content (Join-Path $run schedule.json) -Raw | ConvertFrom-Json)
$output=Join-Path $run $OutputName;New-Item -ItemType Directory $output -Force | Out-Null
$final=[IO.Path]::GetFullPath($Frozen);$binary=Join-Path $final bin/Candidate.exe
$binaryHash=(Get-FileHash $binary).Hash;$sourceHash=(Get-FileHash (Join-Path $final src/Bot.cpp)).Hash
$identityPath=Join-Path $output candidate.json
if(Test-Path $identityPath){$identity=Get-Content $identityPath -Raw | ConvertFrom-Json;if($identity.binary_sha256 -ne $binaryHash -or $identity.source_sha256 -ne $sourceHash){throw 'Candidate changed'}}else{@{binary_sha256=$binaryHash;source_sha256=$sourceHash;frozen=$final;created_utc=[DateTimeOffset]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content $identityPath}
function ReadJson($p){Get-Content -LiteralPath $p -Raw | ConvertFrom-Json}
function SaveProgress{
 $rows=@(Get-ChildItem $output -Filter 'm*.result.json' | Sort-Object Name | ForEach-Object {ReadJson $_.FullName})
 $rows | ConvertTo-Json -Depth 15 | Set-Content (Join-Path $output results.json)
 $rows | Select-Object id,opponent,map,seed,candidate_hosts,outcome,reason,frame,candidate_score,opponent_score,elapsed_seconds,attempt,binary_sha256,evidence | Export-Csv (Join-Path $output results.csv) -NoTypeInformation -Encoding utf8
 $wins=@($rows|Where-Object outcome -eq win).Count;$losses=@($rows|Where-Object outcome -eq loss).Count;$draws=@($rows|Where-Object outcome -eq draw).Count
 $opponents=@($rows | Group-Object opponent | ForEach-Object {@{name=$_.Name;completed=$_.Count;wins=@($_.Group | Where-Object outcome -eq win).Count;losses=@($_.Group | Where-Object outcome -eq loss).Count;draws=@($_.Group | Where-Object outcome -eq draw).Count}})
 $maps=@($rows | Group-Object map | ForEach-Object {@{name=$_.Name;completed=$_.Count;wins=@($_.Group | Where-Object outcome -eq win).Count;losses=@($_.Group | Where-Object outcome -eq loss).Count;draws=@($_.Group | Where-Object outcome -eq draw).Count}})
 @{kind=$protocol.kind;completed=$rows.Count;expected=234;wins=$wins;losses=$losses;draws=$draws;score=$(if($rows.Count -eq 234){100.0*$wins/234}else{$null});score_formula='wins / 234 * 100';by_opponent=$opponents;by_map=$maps;updated_utc=[DateTimeOffset]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $output summary.json)
 Write-Output "$($rows.Count)/234 W=$wins L=$losses D=$draws"
}
Remove-Item Env:STARBENCH_PREFLIGHT_FRAME_CAP -ErrorAction SilentlyContinue
foreach($item in ($schedule | Select-Object -First $Limit)){
 $resultPath=Join-Path $output "$($item.id).result.json";if(Test-Path $resultPath){continue}
 if((Get-FileHash $binary).Hash -ne $binaryHash -or (Get-FileHash (Join-Path $final src/Bot.cpp)).Hash -ne $sourceHash){throw 'Frozen candidate changed'}
 foreach($property in $protocol.critical_hashes.PSObject.Properties){if((Get-FileHash (Join-Path $repo $property.Name)).Hash -ne $property.Value){throw "Fixture changed: $($property.Name)"}}
 $id=$protocol.roster | Where-Object name -eq $item.opponent
 if((Get-FileHash $id.entry).Hash -ne $id.binary_sha256 -or (Get-FileHash $id.bwapi).Hash -ne $id.bwapi_sha256){throw 'Opponent fixture changed'}
 if((Get-FileHash (Join-Path $repo "StarCraft1161/maps/BroodWar/AIIDE/$($item.map)")).Hash -ne $item.map_sha256){throw 'Map changed'}
 $parent=Join-Path $output $item.id;New-Item -ItemType Directory $parent -Force | Out-Null
 $attempts=@(Get-ChildItem $parent -Directory -Filter 'attempt-*').Count
 $accepted=$false
 foreach($number in (($attempts+1)..($attempts+2))){
  $attempt=Join-Path $parent ('attempt-{0:d2}' -f $number)
  # Preserve previous mutable data before restoring the preregistered read fixture.
  foreach($folder in @('read','write')){
   $mutable=Join-Path $id.runtime "bwapi-data/$folder";$resolved=[IO.Path]::GetFullPath($mutable)
   if(!$resolved.StartsWith([IO.Path]::GetFullPath((Join-Path $run runtime)),[StringComparison]::OrdinalIgnoreCase)){throw 'Mutable runtime escaped run root'}
   if(Test-Path $mutable){Move-Item -LiteralPath $mutable -Destination (Join-Path $parent "before-$number-$folder")}
   if($folder -eq 'read'){Copy-Item (Join-Path $run "runtime-initial/$($id.name)/read") $mutable -Recurse}else{New-Item -ItemType Directory $mutable -Force | Out-Null}
  }
  & (Join-Path $PSScriptRoot Match.exe) (Join-Path $final bin) $id.runtime $id.kind $id.entry $item.map $item.seed $item.candidate_hosts $attempt 1800 $id.bwapi
  $receipt=ReadJson (Join-Path $attempt receipt.json);$outcome=$null;$reason=$null;$own=$null;$enemy=$null;$start=$null;$game=$null
  try{
   $start=ReadJson (Join-Path $attempt start.json)
   if($start.map -ne $item.map -or $start.race -ne 'Protoss' -or $start.opponent_race -ne 'Protoss'){throw 'Wrong map or race'}
   if($receipt.status -eq 'complete'){
    $game=ReadJson (Join-Path $attempt result.json);$own=ReadJson (Join-Path $attempt candidate-referee.json);$enemy=ReadJson (Join-Path $attempt opponent-referee.json)
    if($game.reason -eq 'frame_cap_kills_plus_razings'){
     $a=ReadJson (Join-Path $attempt candidate-cap.json);$b=ReadJson (Join-Path $attempt opponent-cap.json)
     if($a.frame -lt 86400 -or $b.frame -lt 86400){throw 'Not final frame cap'}
     $outcome=if($a.score -gt $b.score){'win'}elseif($a.score -lt $b.score){'loss'}else{'draw'};$reason='frame_cap_kills_plus_razings';$own.score=$a.score;$enemy.score=$b.score
    }else{
     if($own.winner -eq $enemy.winner){throw 'Non-complementary MatchEnd'}
     $outcome=if($own.winner){'win'}else{'loss'};$reason='complementary_MatchEnd'
    }
   }elseif($receipt.status -eq 'process_exit'){
    $a=ReadJson (Join-Path $attempt candidate-binding.json);$b=ReadJson (Join-Path $attempt opponent-binding.json)
    $candidateDead=@($receipt.processes|Where-Object {$_.pid -in @($a.game_pid,$a.bot_pid) -and $_.exit_code -ne 259 -and $_.exit_code -ne 0}).Count -gt 0
    $opponentDead=@($receipt.processes|Where-Object {$_.pid -in @($b.game_pid,$b.bot_pid) -and $_.exit_code -ne 259 -and $_.exit_code -ne 0}).Count -gt 0
    if(!$candidateDead -and !$opponentDead){throw 'Exit lacks crash proof'}
    $outcome=if($candidateDead -and $opponentDead){'draw'}elseif($candidateDead){'loss'}else{'win'};$reason='proven_process_crash'
   }
  }catch{$_ | Out-String | Set-Content (Join-Path $attempt adjudication-error.txt);$outcome=$null}
  if($outcome){
   $replays=@(Get-ChildItem $attempt -Filter '*.rep' | ForEach-Object {@{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash}})
   $record=[ordered]@{id=$item.id;opponent=$item.opponent;map=$item.map;map_sha256=$item.map_sha256;seed=$item.seed;candidate_hosts=$item.candidate_hosts;outcome=$outcome;reason=$reason;frame=$game.frame;candidate_score=$own.score;opponent_score=$enemy.score;elapsed_seconds=$receipt.elapsed_seconds;attempt=$number;binary_sha256=$binaryHash;opponent_sha256=$id.binary_sha256;source_sha256=$sourceHash;start=$start;replays=$replays;evidence=$attempt;receipt=$receipt;completed_utc=[DateTimeOffset]::UtcNow.ToString('o')}
   $record | ConvertTo-Json -Depth 15 | Set-Content $resultPath;$accepted=$true;SaveProgress;break
  }
 }
 if(!$accepted){SaveProgress;throw "Unresolved infrastructure failure: $($item.id); evidence retained, resume same schedule after repair"}
}
SaveProgress
