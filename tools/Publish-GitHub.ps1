param()
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$expectedRemote = 'https://github.com/zyd-go/StarCraftAI.git'
if (!(Test-Path -LiteralPath (Join-Path $projectRoot '.git'))) { throw 'The prepared local Git repository is missing.' }
$remote = (& git -C $projectRoot remote get-url origin | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $remote -ne $expectedRemote) { throw 'Unexpected Git remote. Nothing was pushed.' }
$branch = (& git -C $projectRoot branch --show-current | Out-String).Trim()
if ($branch -ne 'main') { throw 'Expected main branch. Nothing was pushed.' }
$pending = @(& git -C $projectRoot status --porcelain)
if ($LASTEXITCODE -ne 0 -or $pending.Count -gt 0) { throw 'Tracked files changed after review. Nothing was pushed.' }
& git -C $projectRoot push -u origin main
if ($LASTEXITCODE -ne 0) { throw 'Git push failed. Complete GitHub sign-in if prompted, then run this script again.' }
$local = (& git -C $projectRoot rev-parse HEAD | Out-String).Trim()
$remoteLine = (& git -C $projectRoot ls-remote origin refs/heads/main | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or !($remoteLine.StartsWith($local + "`t"))) { throw 'Could not verify the remote commit.' }
Write-Output "Uploaded and verified: https://github.com/zyd-go/StarCraftAI"
Write-Output "Commit: $local"