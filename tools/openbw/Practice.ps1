param([Parameter(Mandatory)][string]$Trial,[ValidateSet('Build','Practice')][string]$Action='Practice',[string[]]$Opponents=@(),[ValidateRange(1,12)][int]$Games=6)
$ErrorActionPreference='Stop'
function LinuxPath([string]$Path){
 $full=[IO.Path]::GetFullPath($Path)
 if($full -notmatch '^([A-Za-z]):\\'){throw 'Expected an absolute drive path'}
 '/mnt/'+$Matches[1].ToLower()+'/'+$full.Substring(3).Replace('\','/')
}
$arguments=@('-d','Ubuntu','--','python3',(LinuxPath (Join-Path $PSScriptRoot practice.py)),
 '--trial',(LinuxPath $Trial),'--action',$Action,'--games',"$Games")
if($Opponents.Count){$arguments+=@('--opponents',($Opponents -join ','))}
& wsl @arguments
if($LASTEXITCODE -ne 0){throw "OpenBW $Action failed with code $LASTEXITCODE"}
