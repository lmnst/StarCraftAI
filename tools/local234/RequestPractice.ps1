param([Parameter(Mandatory)][string]$Trial,[string]$Opponent='UAlbertaBot')
$ErrorActionPreference='Stop'
$trialDir=[IO.Path]::GetFullPath($Trial)
$queue=Join-Path $trialDir 'requests';New-Item -ItemType Directory $queue -Force | Out-Null
$token=[guid]::NewGuid().ToString('N')
$request=Join-Path $queue "$token.request.json"
@{opponent=$Opponent;requested_utc=[DateTimeOffset]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content $request
$reply=Join-Path $queue "$token.response.txt"
while(!(Test-Path $reply)){
 $manifest=Get-Content (Join-Path $trialDir 'manifest.json') -Raw | ConvertFrom-Json
 if([DateTimeOffset]::UtcNow -ge [DateTimeOffset]$manifest.deadline_utc){throw 'Deadline reached'}
 Start-Sleep -Seconds 1
}
Get-Content $reply
