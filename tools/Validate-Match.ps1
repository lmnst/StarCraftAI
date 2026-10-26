param([string]$RunDirectory)
$ErrorActionPreference = 'Stop'
$matchesRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\matches'))
if ([string]::IsNullOrWhiteSpace($RunDirectory)) {
    $RunDirectory = Get-Content -LiteralPath (Join-Path $matchesRoot 'latest.txt') -Raw
}
$RunDirectory = $RunDirectory.Trim()
if (![IO.Path]::IsPathRooted($RunDirectory)) { $RunDirectory = Join-Path $matchesRoot $RunDirectory }
$RunDirectory = [IO.Path]::GetFullPath($RunDirectory)
function Read-MatchJson([string]$FileName) { Get-Content -LiteralPath (Join-Path $RunDirectory $FileName) -Raw | ConvertFrom-Json }
$a = Read-MatchJson 'StarterBot-start.json'
$b = Read-MatchJson 'UAlbertaBot-start.json'
$ra = Read-MatchJson 'StarterBot-result.json'
$rb = Read-MatchJson 'UAlbertaBot-result.json'
$pa = Read-MatchJson 'StarterBot-binding.json'
$pb = Read-MatchJson 'UAlbertaBot-binding.json'
$checks = [ordered]@{
    multiplayer = ($a.multiplayer -and $b.multiplayer)
    reciprocal_players = ($a.player -eq 'StarterBot' -and $b.player -eq 'UAlbertaBot' -and $a.enemy -eq $b.player -and $b.enemy -eq $a.player -and $a.player_id -eq $b.enemy_id -and $b.player_id -eq $a.enemy_id)
    same_map = ($a.map_hash -eq $b.map_hash -and $a.map -eq $b.map)
    distinct_game_processes = ($pa.game_pid -ne $pb.game_pid)
    distinct_bot_processes = ($pa.bot_pid -ne $pb.bot_pid)
    manual_unit_input_disabled = (!$a.manual_unit_input -and !$b.manual_unit_input)
    complementary_results = ($ra.winner -ne $rb.winner)
    result_source = ($ra.source -eq 'BWAPI MatchEnd callback' -and $rb.source -eq 'BWAPI MatchEnd callback')
    end_frames_within_120_frames = ($ra.frame -gt 0 -and $rb.frame -gt 0 -and [math]::Abs($ra.frame - $rb.frame) -le 120)
}
$statistics = @()
foreach ($botName in @('StarterBot','UAlbertaBot')) {
    $rows = @(Import-Csv -LiteralPath (Join-Path $RunDirectory "$botName-telemetry.csv"))
    $events = @(Import-Csv -LiteralPath (Join-Path $RunDirectory "$botName-events.csv"))
    $replay = Get-ChildItem -LiteralPath $RunDirectory -Filter "$botName*.rep" | Select-Object -First 1
    $monotonic = $rows.Count -gt 2 -and [int]$rows[0].frame -eq 0
    for ($i=1; $i -lt $rows.Count; $i++) { if ([int]$rows[$i].frame -le [int]$rows[$i-1].frame) { $monotonic = $false } }
    $checks["${botName}_continuous_samples"] = $monotonic
    $checks["${botName}_replay_saved"] = $replay.Length -gt 512
    $checks["${botName}_end_event"] = @($events | Where-Object event -eq 'match_end').Count -eq 1
    $checks["${botName}_final_sample"] = [int]$rows[-1].frame -eq $(if ($botName -eq 'StarterBot') { $ra.frame } else { $rb.frame })
    $statistics += [pscustomobject]@{
        bot = $botName
        samples = $rows.Count
        events = $events.Count
        max_workers = ($rows | ForEach-Object { [int]$_.workers } | Measure-Object -Maximum).Maximum
        max_combat_units = ($rows | ForEach-Object { [int]$_.combat_units } | Measure-Object -Maximum).Maximum
        own_units_destroyed = @($events | Where-Object { $_.event -eq 'destroy' -and [int]$_.owner_id -eq $(if ($botName -eq 'StarterBot') { $a.player_id } else { $b.player_id }) }).Count
        final_sample = $rows[-1]
        replay_bytes = $replay.Length
        replay_sha256 = (Get-FileHash -LiteralPath $replay.FullName -Algorithm SHA256).Hash
    }
}
$passed = @($checks.Values | Where-Object { !$_ }).Count -eq 0
$validation = [ordered]@{
    status = $(if ($passed) {'passed'} else {'failed'})
    winner = $(if ($ra.winner) {'StarterBot'} else {'UAlbertaBot'})
    end_frame = [math]::Max($ra.frame, $rb.frame)
    end_frames = @{StarterBot = $ra.frame; UAlbertaBot = $rb.frame}
    game_seconds_at_24fps = [math]::Round([math]::Max($ra.frame, $rb.frame) / 24, 2)
    run_directory = $RunDirectory
    checks = $checks
    statistics = $statistics
}
$validation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $RunDirectory 'validation.json') -Encoding utf8
$validation | ConvertTo-Json -Depth 8
if (!$passed) { throw 'Match validation failed; inspect checks in validation.json.' }