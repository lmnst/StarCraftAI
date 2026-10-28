param([switch]$Natural)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $repo 'runs/local234-20261026'
$identities=@()
foreach($name in @('Stardust','BananaBrain','Locutus','HladHammer','Steamhammer','UAlbertaBot','Odin','EggBot','Venator','Orion')){
 $fixtureName=if($name -eq 'Locutus'){'Locutus-rebuilt'}else{$name}; $id=Get-Content (Join-Path $run "runtime/$fixtureName/identity.json") -Raw | ConvertFrom-Json
 if($id.bwapi_sha256 -ne 'F2E0F937E9592157656118FA7E5FF30C2327694ED56C1D8F55687972AD97D308'){throw "BWAPI 4.4 fixture mismatch: $name"}
 $identities+=@($id)
}
foreach($mode in 1..3){
 $name=@('LocalZealotRush','LocalFourGateDragoon','LocalDTRush')[$mode-1]
 $id=Get-Content (Join-Path $run 'runtime/UAlbertaBot/identity.json') -Raw | ConvertFrom-Json
 $id.name=$name;$id.entry=Join-Path $run "demos/demo$mode/bin/Candidate.exe";$id.binary_sha256=(Get-FileHash $id.entry).Hash;$id.source='tools/local234/LocalDemo.cpp; locally authored, NOT official demo';$id.upstream_name=$name;$id.upstream_update='2026-10-26 local';$identities+=@($id)
}
$identities | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $run 'roster-candidate.json')
$maps=@('(2)Heartbreak Ridge.scx','(2)Benzene.scx','(2)Destination.scx')
$root=Join-Path $run ('preflight/set-'+(Get-Date -Format yyyyMMdd-HHmmss));New-Item -ItemType Directory $root -Force | Out-Null
$all=@();$i=0
if(!$Natural){$env:STARBENCH_PREFLIGHT_FRAME_CAP='240'}
try{
 foreach($id in $identities){
  $path=Join-Path $root $id.name;$map=$maps[$i%3];$candidateHosts=1-($i%2)
  & (Join-Path $PSScriptRoot 'Match.exe') (Join-Path $run 'baseline/bin') $id.runtime $id.kind $id.entry $map (19261101+$i) $candidateHosts $path $(if($Natural){900}else{150}) $id.bwapi
  $receipt=Get-Content (Join-Path $path 'receipt.json') -Raw | ConvertFrom-Json
  $good=$receipt.status -eq 'complete' -and (Test-Path (Join-Path $path 'candidate-referee.json')) -and (Test-Path (Join-Path $path 'opponent-referee.json'))
  $item=@{opponent=$id.name;map=$map;host=$candidateHosts;passed=$good;path=$path;receipt=$receipt};$all+=@($item)
  $all | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root 'summary.json')
  Write-Output "$($id.name): $good ($($receipt.status), $($receipt.elapsed_seconds)s)"
  $i++
 }
}finally{Remove-Item Env:STARBENCH_PREFLIGHT_FRAME_CAP -ErrorAction SilentlyContinue}
if(@($all | Where-Object {!$_.passed}).Count){throw "Preflight failures: $root"}
Write-Output "All 13 passed: $root"
