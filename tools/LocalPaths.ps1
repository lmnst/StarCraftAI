function Get-LocalPaths {
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $config = if ($env:STARCRAFTAI_PATHS) { $env:STARCRAFTAI_PATHS } else { Join-Path $repo 'local-paths.json' }
    if (!(Test-Path -LiteralPath $config)) {
        throw "Copy local-paths.example.json to $config and set your dependency paths"
    }
    $values = Get-Content -LiteralPath $config -Raw -Encoding utf8 | ConvertFrom-Json
    $paths = @{}
    foreach ($item in $values.PSObject.Properties) {
        $paths[$item.Name] = if ([IO.Path]::IsPathRooted($item.Value)) {
            [IO.Path]::GetFullPath($item.Value)
        } else {
            [IO.Path]::GetFullPath((Join-Path $repo $item.Value))
        }
    }
    return $paths
}
