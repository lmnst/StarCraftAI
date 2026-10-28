param([Parameter(Mandatory)][string]$Trial,[switch]$Harness)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot '../LocalPaths.ps1')
$msbuild=(Get-LocalPaths).msbuild
if($Harness){
 & $msbuild (Join-Path $PSScriptRoot 'Match.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141 /p:WindowsTargetPlatformVersion=10.0.17763.0 /nologo /v:minimal
 if($LASTEXITCODE){throw 'Harness compile failed'}
 return
}
$trialDir=[IO.Path]::GetFullPath($Trial)
$manifestPath=Join-Path $trialDir 'manifest.json'
if(Test-Path $manifestPath){$manifest=Get-Content $manifestPath -Raw | ConvertFrom-Json;if($manifest.phase -eq 'frozen' -or ($manifest.deadline_utc -and [DateTimeOffset]::UtcNow -ge [DateTimeOffset]$manifest.deadline_utc)){throw 'Trial closed'}}
$versions=Join-Path $trialDir 'versions';New-Item -ItemType Directory $versions -Force | Out-Null
$v=Join-Path $versions ('v{0:d3}' -f @(Get-ChildItem $versions -Directory).Count);New-Item -ItemType Directory $v | Out-Null
Copy-Item (Join-Path $trialDir 'src') (Join-Path $v 'source') -Recurse
$start=[DateTimeOffset]::UtcNow
& $msbuild (Join-Path $PSScriptRoot 'Candidate.vcxproj') /t:Rebuild /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141 /p:WindowsTargetPlatformVersion=10.0.17763.0 "/p:TrialDirectory=$trialDir" /nologo /v:minimal *> (Join-Path $v 'build.log')
$exitCode=$LASTEXITCODE
@{started_utc=$start.ToString('o');completed_utc=[DateTimeOffset]::UtcNow.ToString('o');exit_code=$exitCode} | ConvertTo-Json | Set-Content (Join-Path $v 'build.json')
if($exitCode){Get-Content (Join-Path $v 'build.log') -Tail 20;throw 'Compile failed'}
Copy-Item (Join-Path $trialDir 'bin') (Join-Path $v 'bin') -Recurse
Get-ChildItem (Join-Path $v 'source') -File | ForEach-Object {@{file=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}} | ConvertTo-Json | Set-Content (Join-Path $v 'source-sha256.json')
Get-FileHash (Join-Path $v 'bin/Candidate.exe') | Select-Object Hash | ConvertTo-Json | Set-Content (Join-Path $v 'binary.json')
Write-Output "Built $v"
