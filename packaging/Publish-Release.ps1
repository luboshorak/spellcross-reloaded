# Validate and publish a complete Spellcross GitHub release (ZIP + SFX EXE).
# There are no delta patches: the in-game updater downloads the full EXE.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Version,
    [string]$DistDirectory = '',
    [string]$NotesFile = '',
    [switch]$ValidateOnly
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

$sfx = Join-Path $dist ("Spellcross_Reloaded_v{0}.exe" -f $label)
if (-not (Test-Path -LiteralPath $sfx -PathType Leaf)) {
    throw "Full SFX installer missing: $sfx. In-game updates require the complete EXE."
}
# Reject failed WinRAR output or mislabeled ZIP/HTML downloads.
if ((Get-Item -LiteralPath $sfx).Length -lt 1MB) {
    throw "Full SFX installer is unexpectedly small: $sfx"
}
$stream = [System.IO.File]::OpenRead($sfx)
try {
    if ($stream.ReadByte() -ne 77 -or $stream.ReadByte() -ne 90) {
        throw "Full installer does not start with MZ: $sfx"
    }
} finally {
    $stream.Dispose()
}

$assets = New-Object System.Collections.ArrayList
[void]$assets.Add($fullZip)
[void]$assets.Add($sfx)

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
