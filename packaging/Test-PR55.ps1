# Force a verifiable, separate PR #55 test build (Windows PowerShell 5.1+).
# Run from any directory: .\packaging\Test-PR55.ps1
# Optional: -ExistingInstall "C:\Users\...\Games\Spellcross Reloaded"
# Optional: -MovieSource "D:\OldSpellcrossBuild"
[CmdletBinding()]
param(
    [string]$ExistingInstall = '',
    [string]$MovieSource = '',
    [string]$WxRoot = '',
    [switch]$NoLaunch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$expectedBranch = 'fix/strategic-issues-51-54'

$branch = (& git -C $root branch --show-current)
if ($LASTEXITCODE -ne 0) { throw "Cannot read Git branch in $root" }
if ($branch.Trim() -ne $expectedBranch) {
    throw "Wrong branch '$branch'. Run: git fetch origin; git switch $expectedBranch; git pull --ff-only origin $expectedBranch"
}

$dirty = @(& git -C $root status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) { throw 'Cannot check Git status.' }
if ($dirty.Count -gt 0) {
    throw "Tracked files have uncommitted changes. Commit/stash them first (do not delete user work)."
}

$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot obtain Git HEAD.' }
$shortHead = $head.Substring(0, 12)
$target = Join-Path $root ('dist\PR55-R2-' + $shortHead)
Write-Host "Compiling $expectedBranch at $head" -ForegroundColor Cyan

$packager = Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1'
$packArgs = @{
    Rebuild = $true
    NoSfx = $true
    OutputDirectory = $target
}
if ($MovieSource) { $packArgs.MovieSource = $MovieSource }
if ($WxRoot) { $packArgs.WxRoot = $WxRoot }
& $packager @packArgs

$stage = Get-ChildItem -LiteralPath $target -Directory |
    Where-Object { $_.Name -like 'Spellcross-Reloaded-Win64-*' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $stage) { throw "No built game staging directory found in $target" }

$exe = Join-Path $stage.FullName 'Spellcross.exe'
$manifest = Join-Path $stage.FullName 'BUILD-INFO.txt'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Game executable not found: $exe" }
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw "Build manifest not found: $manifest" }
$manifestContent = Get-Content -LiteralPath $manifest -Raw
if (-not $manifestContent.Contains($head)) { throw 'Build manifest does not match Git HEAD.' }

# Copy preexisting game state *only* to the new isolated test directory.
# Never change or delete the original installation.
if ($ExistingInstall) {
    $old = (Resolve-Path -LiteralPath $ExistingInstall).Path
    if ($old -eq $stage.FullName) { throw 'Source and destination install are identical.' }
    foreach ($file in @('config.ini')) {
        $src = Join-Path $old $file
        if (Test-Path -LiteralPath $src -PathType Leaf) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $stage.FullName $file) -Force
        }
    }
    foreach ($folder in @('game_data', 'save')) {
        $src = Join-Path $old $folder
        if (Test-Path -LiteralPath $src -PathType Container) {
            Copy-Item -LiteralPath $src -Destination $stage.FullName -Recurse -Force
        }
    }
}

$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host ""
Write-Host "READY: exact game binary: $exe" -ForegroundColor Green
Write-Host "Branch: $expectedBranch"
Write-Host "Commit: $head"
Write-Host "SHA256: $hash"
Write-Host "Manifest: $manifest"
Write-Host "Look for [PR55-R2] in the window title. If absent, a different EXE is running." -ForegroundColor Yellow

if (-not $NoLaunch) {
    Start-Process -FilePath $exe -WorkingDirectory $stage.FullName
}
