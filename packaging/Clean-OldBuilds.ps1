# Safely clean old generated combined releases from dist/.
# Dry run by default; add -Apply to delete. Keeps latest 3 release timestamps.
# Never touches unrelated files or packaged directories containing user data.
[CmdletBinding()]
param(
    [string]$DistDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'dist'),
    [ValidateRange(1, 100)]
    [int]$Keep = 3,
    [switch]$Apply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $DistDirectory -PathType Container)) {
    Write-Host "No dist folder found: $DistDirectory"
    return
}
$dist = (Resolve-Path -LiteralPath $DistDirectory).Path
$builds = @{}
$pattern = '^Spellcross-Reloaded-Win64-(\d{8}-\d{6})$'

foreach ($item in (Get-ChildItem -LiteralPath $dist -Force)) {
    $buildName = if ($item.PSIsContainer) {
        $item.Name
    } elseif ($item.Extension -ieq '.zip') {
        $item.BaseName
    } else {
        continue
    }

    if ($buildName -notmatch $pattern) { continue }
    $stamp = $Matches[1]
    if (-not $builds.ContainsKey($stamp)) {
        $builds[$stamp] = @()
    }
    $builds[$stamp] += $item
}

$stamps = @($builds.Keys | Sort-Object -Descending)
if ($stamps.Count -le $Keep) {
    Write-Host "Found $($stamps.Count) build(s). Keeping all; limit is $Keep."
    return
}

Write-Host ("Found {0} builds; keeping the {1} newest." -f $stamps.Count, $Keep)
$removed = 0
$protected = 0

foreach ($stamp in ($stamps | Select-Object -Skip $Keep)) {
    foreach ($item in $builds[$stamp]) {
        if ($item.PSIsContainer) {
            # Only a generated staging directory qualifies. A folder used to
            # run the game may contain personal saves, imported archives or
            # configuration: preserve that folder.
            $gameExe = Join-Path $item.FullName 'Spellcross.exe'
            $readme = Join-Path $item.FullName 'README-FIRST.txt'
            $editorExe = Join-Path $item.FullName 'Editor\spellcross_map_editor.exe'
            $isStaging = (Test-Path -LiteralPath $gameExe -PathType Leaf) -and
                         (Test-Path -LiteralPath $readme -PathType Leaf) -and
                         (Test-Path -LiteralPath $editorExe -PathType Leaf)
            $hasUserData = $false
            foreach ($relative in @('config.ini', 'game_data', 'save', 'temp', 'Editor\config.ini')) {
                if (Test-Path -LiteralPath (Join-Path $item.FullName $relative)) {
                    $hasUserData = $true
                    break
                }
            }
            if (-not $isStaging -or $hasUserData) {
                Write-Warning ("Skipping nonstandard or user-modified build: " + $item.FullName)
                $protected++
                continue
            }
        }

        if ($Apply) {
            Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop
            Write-Host "Deleted: $($item.Name)"
        } else {
            Write-Host "Would delete: $($item.Name)"
        }
        $removed++
    }
}

if ($Apply) {
    Write-Host "Deleted $removed old ZIP(s)/staging folder(s); protected $protected folder(s)." -ForegroundColor Green
} else {
    Write-Host "Preview: $removed item(s) eligible, $protected folder(s) protected. Add -Apply to delete." -ForegroundColor Yellow
}
