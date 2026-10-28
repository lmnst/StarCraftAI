param([Parameter(Mandatory)][string]$Name,[string]$PackageName=$Name,[switch]$LocalUAlberta)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo 'benchmark/local234-20261026'
$package=Join-Path $fixture "packages/$PackageName"
$runtime=Join-Path $repo "runs/local234-20261026/runtime/$Name"
if(Test-Path $runtime){throw 'Runtime already exists'}
New-Item -ItemType Directory $runtime -Force | Out-Null
Get-ChildItem (Join-Path $repo 'StarCraft1161') -File | Where-Object {$_.Extension -in '.exe','.dll','.snp','.MPQ','.mpq','.icc'} | Copy-Item -Destination $runtime
Copy-Item (Join-Path $repo 'StarCraft1161/maps') (Join-Path $runtime 'maps') -Recurse
foreach($d in @('AI','read','write')){New-Item -ItemType Directory (Join-Path $runtime "bwapi-data/$d") -Force | Out-Null}
$bwapi=Join-Path $runtime 'bwapi-data/BWAPI.dll'
if($LocalUAlberta){
 Copy-Item (Join-Path $repo 'StarCraft1161/bwapi-data/BWAPI.dll') $bwapi
 Copy-Item (Join-Path $repo 'src/UAlbertaBot/UAlbertaBot/bin/*') $runtime -Recurse
 $entry=Join-Path $runtime 'UAlbertaBot.exe';$kind='exe';$meta=@{name='UAlbertaBot';update='local patched build';botBinary='See THIRD_PARTY.md and existing patches';race='Protoss'}
}else{
 $meta=Get-Content (Join-Path $package 'metadata.json') -Raw | ConvertFrom-Json
 $zip=Join-Path $package 'bot.zip';if(!(Test-Path $zip) -or (Get-Item $zip).Length -eq 0){$zip=Join-Path $package 'bot-retry.zip'}
 $api=Join-Path $package 'BWAPI.dll';if(!(Test-Path $api) -or (Get-Item $api).Length -eq 0){$api=Join-Path $package 'BWAPI-retry.dll'}
 if((Get-Item $zip).Length -eq 0 -or (Get-Item $api).Length -eq 0){throw 'Empty package'}
 $unpacked=Join-Path $package 'ready';Expand-Archive -LiteralPath $zip -DestinationPath $unpacked -Force
 Copy-Item $api $bwapi
 Copy-Item (Join-Path $unpacked '*') (Join-Path $runtime 'bwapi-data/AI') -Recurse
 Copy-Item (Join-Path $unpacked '*') (Join-Path $runtime 'bwapi-data/read') -Recurse
 $binary=Get-ChildItem $unpacked -Recurse -File | Where-Object {$_.Extension -eq $(if($meta.botType -eq 'EXE'){'.exe'}else{'.dll'})} | Select-Object -First 1
 if(!$binary){throw 'Entry not found'}
 $entry=Join-Path (Join-Path $runtime 'bwapi-data/AI') $binary.FullName.Substring($unpacked.Length+1)
 $kind=if($meta.botType -eq 'EXE'){'exe'}else{'dll'}
}
$record=[ordered]@{name=$Name;runtime=$runtime;kind=$kind;entry=$entry;bwapi=$bwapi;binary_sha256=(Get-FileHash $entry).Hash;bwapi_sha256=(Get-FileHash $bwapi).Hash;source=$meta.botBinary;upstream_name=$meta.name;upstream_update=$meta.update;forced_race='Protoss';upstream_race=$meta.race;official_version_match='unconfirmed'}
$record | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $runtime 'identity.json')
$record | ConvertTo-Json -Compress
