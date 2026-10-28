param([Parameter(Mandatory)][string]$Trial,[Parameter(Mandatory)][string]$Request)
$ErrorActionPreference='Stop'
$reply=$Request.Replace('.request.json','.working.txt')
try{
 $r=Get-Content $Request -Raw | ConvertFrom-Json
 & (Join-Path $PSScriptRoot 'Practice.ps1') -Trial $Trial -Opponent $r.opponent *> $reply
}catch{$_ | Out-String | Add-Content $reply;exit 1}

finally{Move-Item -LiteralPath $reply -Destination ($Request.Replace('.request.json','.response.txt'))}
