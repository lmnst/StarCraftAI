param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceRoot = Join-Path $projectRoot 'src'
$lock = Get-Content -LiteralPath (Join-Path $projectRoot 'patches\sources.lock.json') -Raw | ConvertFrom-Json
if (!(Get-Command git -ErrorAction SilentlyContinue)) { throw 'Install Git for Windows first.' }
function Test-GitCommand([string[]]$GitArguments) {
    $ErrorActionPreference = 'SilentlyContinue'
    & git @GitArguments *> $null
    return $LASTEXITCODE -eq 0
}
foreach ($dependency in $lock.dependencies) {
    $target = Join-Path $sourceRoot $dependency.name
    $patch = Join-Path (Join-Path $projectRoot 'patches') $dependency.patch
    if (!(Test-Path -LiteralPath (Join-Path $target '.git'))) {
        if ($CheckOnly) { throw "Source missing: $target" }
        if (Test-Path -LiteralPath $target) { throw "Existing non-Git directory will not be overwritten: $target" }
        [IO.Directory]::CreateDirectory($sourceRoot) | Out-Null
        & git init $target
        if ($LASTEXITCODE -ne 0) { throw 'git init failed' }
        & git -C $target remote add origin $dependency.url
        if ($LASTEXITCODE -ne 0) { throw 'git remote add failed' }
        & git -C $target fetch --depth 1 origin $dependency.commit
        if ($LASTEXITCODE -ne 0) { throw 'Fetching the pinned source commit failed' }
        & git -C $target checkout --detach FETCH_HEAD
        if ($LASTEXITCODE -ne 0) { throw 'Checking out the source commit failed' }
    }
    $head = (& git -C $target rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $dependency.commit) {
        throw "Unexpected source revision for $($dependency.name). Existing files were not reset."
    }
    if (Test-GitCommand @('-C',$target,'apply','--reverse','--check','--ignore-space-change',$patch)) {
        Write-Output "$($dependency.name): pinned revision and local patch verified."
        continue
    }
    if (!(Test-GitCommand @('-C',$target,'apply','--check','--ignore-space-change',$patch))) {
        throw "Patch conflicts with existing local files: $($dependency.name). No reset was performed."
    }
    if ($CheckOnly) { throw "Patch is not applied: $($dependency.name). Run without -CheckOnly to apply it." }
    & git -C $target apply --ignore-space-change $patch
    if ($LASTEXITCODE -ne 0) { throw "Patch failed: $($dependency.name)" }
    Write-Output "$($dependency.name): pinned source prepared and patch applied."
}
Write-Output 'Source preparation complete. Next: tools\BuildBots.cmd'