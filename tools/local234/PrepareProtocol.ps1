$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $repo 'runs/local234-20261026'
$path=Join-Path $run protocol.json
if(Test-Path $path){throw 'Protocol already fixed'}
$roster=Get-Content (Join-Path $run roster-candidate.json) -Raw | ConvertFrom-Json
$maps=foreach($name in @('(2)Heartbreak Ridge.scx','(2)Benzene.scx','(2)Destination.scx')){@{name=$name;sha256=(Get-FileHash (Join-Path $repo "StarCraft1161/maps/BroodWar/AIIDE/$name")).Hash}}
$schedule=@();$seedSet=[Collections.Generic.HashSet[uint32]]::new();$random=[Security.Cryptography.RandomNumberGenerator]::Create()
foreach($bot in $roster){foreach($map in $maps){foreach($repeat in 1..6){
 do{$bytes=[byte[]]::new(4);$random.GetBytes($bytes);$seed=[BitConverter]::ToUInt32($bytes,0) -band 0x7fffffff}while($seed -in @(20261026,19261101) -or !$seedSet.Add($seed))
 $schedule+=@([ordered]@{id=('m{0:d3}' -f ($schedule.Count+1));opponent=$bot.name;map=$map.name;map_sha256=$map.sha256;seed=$seed;candidate_hosts=($repeat%2);repeat=$repeat})
}}}
if($schedule.Count -ne 234 -or $seedSet.Count -ne 234){throw 'Schedule size mismatch'}
$critical=@{}
foreach($f in @('tools/local234/Match.exe','tools/local234/Match.cpp','tools/local234/Referee.dll','tools/local234/Referee.cpp','tools/local234/Evaluate.ps1','tools/local234/Client.cpp','tools/local234/Bot.h','tools/local234/Candidate.vcxproj','tools/local234/Build.ps1','tools/local234/Practice.ps1','tools/local234/RequestPractice.ps1','tools/local234/PracticeWorker.ps1','StarCraft1161/StarCraft.exe','StarCraft1161/STARDAT.MPQ','StarCraft1161/BROODAT.MPQ','StarCraft1161/patch_rt.mpq','StarCraft1161/bwapi-data/BWAPI.dll')){$critical[$f]=(Get-FileHash (Join-Path $repo $f)).Hash}
foreach($id in $roster){
 $initial=Join-Path $run "runtime-initial/$($id.name)";New-Item -ItemType Directory $initial -Force | Out-Null
 Copy-Item (Join-Path $id.runtime bwapi-data/read) (Join-Path $initial read) -Recurse
}
$protocol=[ordered]@{kind='local reproduction; NOT official StarSkirmish score';version=1;fixed_utc=[DateTimeOffset]::UtcNow.ToString('o');engine='native StarCraft 1.16.1 + BWAPI 4.4.0';source='https://starskirmish.com/bench/v0-2/';official_versions='unconfirmed';frame_cap=86400;cutoff='first callback at or after frame 86400, both own-player scores independently captured';score='wins / 234 * 100; draws count zero';pairing='six distinct preregistered fresh seeds; candidate hosts alternating three each; actual spawn chosen by engine RNG and recorded, not guaranteed balanced';natural_end='complementary BWAPI MatchEnd outcomes from both trusted tournament modules';cap='higher own kills + own razings from the two independent cap records wins; equality draw';crash='after game start: a single candidate bot/game crash is loss; single opponent bot/game crash is win; simultaneous crash draw. Retain exit codes. Missing proof stays unresolved infrastructure error';infrastructure='startup, missing replay, timeout, mismatched callbacks or missing referee evidence are unresolved; max two automatic attempts per item, same seed; halt if both fail; no dropped denominator entries';learning='read directory reset to frozen fixture before every evaluation match; write outputs archived';differences=@('5 official same-name human bots but exact official versions unconfirmed','5 other public human bots replace unavailable/incompatible roster entries','3 local demo implementations replace unavailable official demo sources','native engine replaces OpenBW','local mining skeleton and tools differ; FAP/BWEM available; isolation partially cooperative','single Codex run, not five-run model mean');roster=$roster;maps=$maps;critical_hashes=$critical}
$schedule | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run schedule.json)
$protocol | ConvertTo-Json -Depth 12 | Set-Content $path
Get-FileHash $path,(Join-Path $run schedule.json) | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $run protocol-sha256.json)
Write-Output "Fixed 13 opponents, 3 maps, 234 unique items/seeds: $path"
