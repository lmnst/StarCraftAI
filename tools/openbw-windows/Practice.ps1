param([Parameter(Mandatory)][string]$Trial,[ValidateSet('Build','Practice','Evaluate','Preflight')][string]$Action='Practice',[string[]]$Opponents=@(),[ValidateRange(1,12)][int]$Games=6)
$ErrorActionPreference='Stop'
$arguments=@((Join-Path $PSScriptRoot 'practice.py'),'--trial',[IO.Path]::GetFullPath($Trial),'--action',$Action,'--games',"$Games")
if($Opponents.Count){$arguments+=@('--opponents',($Opponents -join ','))}
& E:/anaconda_c/python.exe @arguments
if($LASTEXITCODE -ne 0){throw "Windows OpenBW $Action failed: $LASTEXITCODE"}
