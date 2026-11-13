# Validate and publish one complete Spellcross GitHub release.
# Refuses a normal publication unless the full ZIP and both delta assets agree.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Version,
    [string]$DistDirectory = '',
    [string]$NotesFile = '',
    [switch]$ValidateOnly,
    [switch]$FullOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if ($Version -notmatch '^(?:v)?(?<label>\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?)$') {
    throw "Invalid release version: $Version"
}
$label = $Matches['label']
$tag = 'v' + $label
if (-not $DistDirectory) { $DistDirectory = Join-Path $root 'dist' }
$dist = (Resolve-Path -LiteralPath $DistDirectory).Path
$fullZip = Join-Path $dist ("Spellcross_Reloaded_v{0}.zip" -f $label)
if (-not (Test-Path -LiteralPath $fullZip -PathType Leaf)) {
    throw "Full release ZIP missing: $fullZip"
}

$assets = New-Object System.Collections.ArrayList
[void]$assets.Add($fullZip)
$sfx = Join-Path $dist ("Spellcross_Reloaded_v{0}.exe" -f $label)
if (Test-Path -LiteralPath $sfx -PathType Leaf) { [void]$assets.Add($sfx) }

if (-not $FullOnly) {
    $manifestPath = Join-Path $dist 'patch-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw 'patch-manifest.json is mandatory. Rebuild with the previous release stage.'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ([int]$manifest.schema -ne 1 -or [string]$manifest.to_version -ne $label) {
        throw "Patch manifest does not target $label."
    }
    $from = [string]$manifest.from_version
    if ($from -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?$' -or $from -eq $label) {
        throw "Invalid patch source version: $from"
    }
    $expectedArchive = "Spellcross_Patch_v${from}_to_v${label}.zip"
    if ([string]$manifest.archive -ne $expectedArchive) {
        throw "Patch manifest archive mismatch: expected $expectedArchive"
    }
    $patchZip = Join-Path $dist $expectedArchive
    if (-not (Test-Path -LiteralPath $patchZip -PathType Leaf)) {
        throw "Patch ZIP missing: $patchZip"
    }
    $actualHash = (Get-FileHash -LiteralPath $patchZip -Algorithm SHA256).Hash
    if ($actualHash -ne [string]$manifest.archive_sha256) {
        throw "Patch ZIP SHA256 does not match patch-manifest.json: $expectedArchive"
    }
    [void]$assets.Add($manifestPath)
    [void]$assets.Add($patchZip)
}

if (-not $NotesFile) { $NotesFile = Join-Path $root ("packaging\releases\v{0}.md" -f $label) }
if (-not (Test-Path -LiteralPath $NotesFile -PathType Leaf)) {
    throw "Release notes missing: $NotesFile"
}

Write-Host ("Validated {0}: {1}" -f $tag, (($assets | ForEach-Object { Split-Path -Leaf $_ }) -join ', ')) -ForegroundColor Green
if ($ValidateOnly) { exit 0 }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'GitHub CLI (gh) is required to publish.' }

$assetArguments = @($assets.ToArray())
& gh release create $tag @assetArguments --repo 'luboshorak/spellcross-reloaded' `
    --verify-tag --title ("Spellcross Reloaded {0}" -f $tag) --notes-file $NotesFile
if ($LASTEXITCODE -ne 0) { throw "GitHub release publication failed: $tag" }

$published = & gh release view $tag --repo 'luboshorak/spellcross-reloaded' --json assets | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw "Could not verify published release: $tag" }
$publishedNames = @($published.assets | ForEach-Object { [string]$_.name })
foreach ($assetPath in $assets) {
    $name = Split-Path -Leaf $assetPath
    if ($publishedNames -notcontains $name) { throw "Published release is missing asset: $name" }
}
Write-Host "Published and verified $tag." -ForegroundColor Green
