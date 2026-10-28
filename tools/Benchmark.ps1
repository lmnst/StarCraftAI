param(
    [Parameter(Mandatory)][ValidateSet('Prepare','Start','Build','Practice','Finish')][string]$Action,
    [Parameter(Mandatory)][string]$Trial,
    [int]$BudgetMinutes = 10,
    [string]$Agent = 'current Codex conversation; prior context uncontrolled',
    [uint32]$Seed = 20261026,
    [int]$MatchTimeoutSeconds = 300,
    [string]$MSBuild = $env:MSBUILD_EXE_PATH,
    [string]$Toolset = 'v143',
    [string]$WindowsSDK = '10.0'
)
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($Trial -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$') { throw 'Trial must be a simple directory name.' }
$trialDir = Join-Path $project "runs\$Trial"
$manifestPath = Join-Path $trialDir 'manifest.json'
$source = Join-Path $trialDir 'src\starterbot'
function Save-Json($Value, [string]$Path) { $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding utf8 }
function Hash-Source([string]$Directory) {
    $result = [ordered]@{}
    Get-ChildItem -LiteralPath $Directory -File | Sort-Object Name | ForEach-Object { $result[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    return $result
}
function Hash-Fixtures {
    $result = [ordered]@{}
    foreach ($file in @('src/UAlbertaBot/UAlbertaBot/bin/UAlbertaBot.exe',
        'src/STARTcraft/visualstudio/Win32/Release/BWAPI/BWAPI.lib',
        'StarCraft1161/StarCraft.exe','StarCraft1161/bwapi-data/BWAPI.dll',
        'StarCraft1161/maps/BroodWar/AIIDE/(2)Heartbreak Ridge.scx',
        'tools/BenchmarkMatch.exe','tools/BenchmarkMatch.cpp','tools/MatchLauncher.cpp',
        'tools/BenchmarkBot.vcxproj','tools/Benchmark.ps1','tools/Telemetry.h','tools/RuntimePaths.h',
        'tools/Validate-Match.ps1','benchmark/task.md')) {
        $result[$file] = (Get-FileHash -LiteralPath (Join-Path $project $file) -Algorithm SHA256).Hash
    }
    return $result
}
function Comparable($Value) { return ($Value | ConvertTo-Json -Depth 12 -Compress) }
function Check-Deadline {
    if ($manifest.phase -eq 'finished') { throw 'Trial already frozen.' }
    if ($manifest.deadline_utc -and [DateTimeOffset]::UtcNow -ge [DateTimeOffset]$manifest.deadline_utc) { throw 'Agent time budget expired. Freeze the latest built version with Finish.' }
}
if ($Action -eq 'Prepare') {
    if (Test-Path -LiteralPath $trialDir) { throw 'Trial already exists; choose a new name.' }
    if ($BudgetMinutes -lt 1) { throw 'BudgetMinutes must be positive.' }
    $initial = Join-Path $project 'src\STARTcraft\src\starterbot'
    $expected = Get-Content -LiteralPath (Join-Path $project 'benchmark\initial-source-sha256.json') -Raw | ConvertFrom-Json
    if ((Comparable (Hash-Source $initial)) -ne (Comparable $expected)) { throw 'Initial source differs from the pinned baseline.' }
    New-Item -ItemType Directory -Path $source -Force | Out-Null
    Copy-Item -Path (Join-Path $initial '*') -Destination $source
    Copy-Item -LiteralPath (Join-Path $project 'benchmark\task.md') -Destination (Join-Path $trialDir 'task.md')
    New-Item -ItemType Directory -Path (Join-Path $trialDir 'initial') | Out-Null
    Copy-Item -Path (Join-Path $source '*') -Destination (Join-Path $trialDir 'initial')
    $manifest = [ordered]@{
        trial = $Trial; kind = 'local workflow reproduction, NOT official score'; phase = 'prepared'
        agent = $Agent; budget_minutes = $BudgetMinutes; started_utc = $null; deadline_utc = $null
        parent_commit = (& git -C $project rev-parse HEAD); cli_version = (& codex --version | Select-Object -Last 1)
        allowed_edits = 'src/starterbot/* inside this trial only'
        tools = @('read local source/API documentation','edit candidate source','Build','Practice','Finish')
        restrictions = @('no web during timed trial','no opponent source access','no runtime/tool/opponent changes','no other agents')
        enforcement = 'cooperative tools with deadline checks, not an OS isolation boundary'
        engine = 'native StarCraft 1.16.1 + BWAPI 4.4.0; not official OpenBW harness'
        opponent = 'UAlbertaBot'; map = '(2)Heartbreak Ridge.scx'; practice_seed = $Seed
        initial_source = (Hash-Source $source); fixtures = (Hash-Fixtures)
        latest_version = $null; matches = @()
    }
    Save-Json $manifest $manifestPath
    Write-Output "Prepared $trialDir. Build and Practice once for baseline, then Start the agent timer."
    return
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ((Comparable (Hash-Fixtures)) -ne (Comparable $manifest.fixtures)) { throw 'Fixed runtime/tools changed since Prepare; create a new trial.' }
if ($Action -eq 'Start') {
    if ($manifest.phase -ne 'prepared') { throw 'Timer can only start once.' }
    $manifest.started_utc = [DateTime]::UtcNow.ToString('o')
    $manifest.deadline_utc = [DateTime]::UtcNow.AddMinutes($manifest.budget_minutes).ToString('o')
    $manifest.phase = 'agent'
    Save-Json $manifest $manifestPath
    Write-Output "Deadline UTC: $($manifest.deadline_utc)"
    return
}
if ($Action -ne 'Finish') { Check-Deadline }
if ($Action -eq 'Build') {
    if (!$MSBuild) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $MSBuild = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    }
    if (!$MSBuild) { throw 'Pass -MSBuild with the installed MSBuild.exe path.' }
    $versions = Join-Path $trialDir 'versions'
    New-Item -ItemType Directory -Path $versions -Force | Out-Null
    $version = 'v{0:d3}' -f @(Get-ChildItem -LiteralPath $versions -Directory).Count
    $versionDir = Join-Path $versions $version
    New-Item -ItemType Directory -Path (Join-Path $versionDir 'source') -Force | Out-Null
    Copy-Item -Path (Join-Path $source '*') -Destination (Join-Path $versionDir 'source')
    Save-Json (Hash-Source $source) (Join-Path $versionDir 'source-sha256.json')
    $diff = & git --no-pager diff --no-index -- (Join-Path $trialDir 'initial') (Join-Path $versionDir 'source') 2>&1
    [IO.File]::WriteAllText((Join-Path $versionDir 'changes.patch'), ($diff -join [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
    $LASTEXITCODE = 0 # git diff returns 1 when changes exist.
    $began = [DateTime]::UtcNow
    & $MSBuild (Join-Path $PSScriptRoot 'BenchmarkBot.vcxproj') /t:Rebuild /m:1 /nr:false /p:Configuration=Release /p:Platform=Win32 "/p:PlatformToolset=$Toolset" "/p:WindowsTargetPlatformVersion=$WindowsSDK" "/p:TrialDirectory=$trialDir" /verbosity:minimal /nologo *> (Join-Path $versionDir 'build.log')
    $buildExit = $LASTEXITCODE
    Save-Json ([ordered]@{exit_code=$buildExit; started_utc=$began.ToString('o'); completed_utc=[DateTime]::UtcNow.ToString('o'); msbuild=$MSBuild; toolset=$Toolset; windows_sdk=$WindowsSDK}) (Join-Path $versionDir 'build.json')
    if ($buildExit -ne 0) { throw "Build failed; read $versionDir\build.log" }
    New-Item -ItemType Directory -Path (Join-Path $versionDir 'bin') | Out-Null
    Copy-Item -LiteralPath (Join-Path $trialDir 'bin\StarterBot.exe') -Destination (Join-Path $versionDir 'bin\StarterBot.exe')
    Save-Json @{sha256=(Get-FileHash -LiteralPath (Join-Path $versionDir 'bin\StarterBot.exe')).Hash} (Join-Path $versionDir 'binary.json')
    # A compile that overran the budget is retained as evidence, but cannot become the submission.
    Check-Deadline
    $manifest.latest_version = $version
    Save-Json $manifest $manifestPath
    Write-Output "Built $version; source snapshot, diff, compiler log and executable saved."
    return
}
if (!$manifest.latest_version) { throw 'Build a candidate first.' }
$selected = Join-Path $trialDir "versions\$($manifest.latest_version)"
if ($Action -eq 'Practice') {
    if ($Seed -ne $manifest.practice_seed) { throw 'Practice seed is fixed in the trial manifest.' }
    $built = Get-Content -LiteralPath (Join-Path $selected 'source-sha256.json') -Raw | ConvertFrom-Json
    if ((Comparable (Hash-Source $source)) -ne (Comparable $built)) { throw 'Source changed after Build. Build the current source first.' }
    $binary = Get-Content -LiteralPath (Join-Path $selected 'binary.json') -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath (Join-Path $selected 'bin\StarterBot.exe')).Hash -ne $binary.sha256) { throw 'Built candidate was modified.' }
    if ($MatchTimeoutSeconds -lt 1) { throw 'Match timeout must be positive.' }
    if ($manifest.deadline_utc) {
        $remaining = [math]::Floor(([DateTimeOffset]$manifest.deadline_utc - [DateTimeOffset]::UtcNow).TotalSeconds)
        $MatchTimeoutSeconds = [math]::Min($MatchTimeoutSeconds, $remaining)
    }
    $number = @($manifest.matches).Count
    $receiptPath = Join-Path $trialDir ("match-{0:d3}.json" -f $number)
    & (Join-Path $PSScriptRoot 'BenchmarkMatch.exe') (Join-Path $selected 'bin') $Seed $MatchTimeoutSeconds $receiptPath
    $matchExit = $LASTEXITCODE
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    $record = [ordered]@{version=$manifest.latest_version; phase=$manifest.phase; seed=$Seed; status=$receipt.status; elapsed_seconds=$receipt.elapsed_seconds; match_directory=$receipt.match_directory; feedback=$null}
    if ($matchExit -eq 0) {
        try {
            $validation = & (Join-Path $PSScriptRoot 'Validate-Match.ps1') -RunDirectory $receipt.match_directory | ConvertFrom-Json
            $record.status = $validation.status
            $record.feedback = [ordered]@{winner=$validation.winner; game_seconds=$validation.game_seconds_at_24fps; statistics=$validation.statistics}
        } catch { $record.status = 'validation_failed'; $record.feedback = $_.Exception.Message }
    }
    $manifest.matches = @($manifest.matches) + @($record)
    Save-Json $record (Join-Path $selected ("feedback-{0:d3}.json" -f $number))
    Save-Json $manifest $manifestPath
    $record | ConvertTo-Json -Depth 8
    if ($record.status -ne 'passed') { throw 'Practice did not produce a validated completed match; this is not a scored win/loss.' }
    return
}
if ($Action -eq 'Finish') {
    if ($manifest.phase -eq 'finished') { throw 'Trial already frozen.' }
    New-Item -ItemType Directory -Path (Join-Path $trialDir 'final') | Out-Null
    Copy-Item -LiteralPath $selected -Destination (Join-Path $trialDir 'final\candidate') -Recurse
    $manifest.phase = 'finished'
    $manifest | Add-Member -NotePropertyName finished_utc -NotePropertyValue ([DateTime]::UtcNow.ToString('o'))
    Save-Json $manifest $manifestPath
    Write-Output "Frozen $($manifest.latest_version). Local practice only; no held-out or official score."
}
