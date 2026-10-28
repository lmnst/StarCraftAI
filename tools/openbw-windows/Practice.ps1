param([Parameter(Mandatory)][string]$Trial,[ValidateSet('Build','Practice','Evaluate','Preflight')][string]$Action='Practice',[string[]]$Opponents=@(),[ValidateRange(1,12)][int]$Games=6)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../LocalPaths.ps1')
$paths=Get-LocalPaths
$arguments=@((Join-Path $PSScriptRoot 'practice.py'),'--trial',[IO.Path]::GetFullPath($Trial),'--action',$Action,'--games',"$Games")
if($Opponents.Count){$arguments+=@('--opponents',($Opponents -join ','))}
& $paths.python @arguments
if($LASTEXITCODE -ne 0){throw "Windows OpenBW $Action failed: $LASTEXITCODE"}
