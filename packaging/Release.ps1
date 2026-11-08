# Spellcross Reloaded – one-command release version setup.
# Usage: .\packaging\Release.ps1 [-Version 0.9.6] [-NoSfx] [-WxRoot ...]
# Windows PowerShell 5.1+. Version metadata is temporarily injected and ALWAYS
# restored byte-for-byte, even if compilation/packaging fails.
[CmdletBinding()]
param(
    [string]$Version = '',
    [switch]$Rebuild,
    [string]$WxRoot = '',
    [string]$OutputDirectory = '',
    [string]$MovieSource = '',
    [switch]$NoZip,
    [switch]$NoSfx,
    [string]$WinRARPath = '',
    [switch]$PruneOldBuilds,
    [switch]$ValidateOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$headerPath = Join-Path $root 'spellcross-map-edit-main\source\app_version.h'
$resourcesPath = Join-Path $root 'spellcross-map-edit-main\source\resources.rc'
foreach ($path in @($headerPath, $resourcesPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required release metadata file not found: $path"
    }
}

# The input is intentionally separate from the version checked into main.
# This makes repeated 'git pull --ff-only' and subsequent releases painless.
if ([string]::IsNullOrWhiteSpace($Version)) {
    if ($ValidateOnly) { throw 'Supply -Version for non-interactive -ValidateOnly.' }
    $Version = Read-Host 'Release version (e.g. 0.9.6 or 0.9.6-RC1)'
}
$Version = $Version.Trim()

# The Windows resource file version has four numeric segments, regardless of
# whether the public tag has a fourth segment or a prerelease suffix.
$pattern = '^(?:v)?(?<major>0|[1-9][0-9]*)\.(?<minor>0|[1-9][0-9]*)\.(?<patch>0|[1-9][0-9]*)(?:\.(?<build>0|[1-9][0-9]*))?(?<suffix>-(?:RC|beta)(?:0|[1-9][0-9]*))?$'
$parsed = [regex]::Match($Version, $pattern)
if (-not $parsed.Success) {
    throw "Invalid release version '$Version'. Examples: 0.9.6, 0.9.4.2, 0.9.6-RC1. Do not include spaces or arbitrary suffixes."
}
$numbers = @()
foreach ($part in @('major','minor','patch','build')) {
    $value = if ($part -eq 'build' -and -not $parsed.Groups[$part].Success) {
        '0'
    } else {
        $parsed.Groups[$part].Value
    }
    $number = 0
    if (-not [int]::TryParse($value, [ref]$number) -or $number -gt 65535) {
        throw "Windows file-version component '$part' is out of range (0-65535): $value"
    }
    $numbers += $number
}
$numericVersion = $numbers -join '.'
$releaseLabel = ('{0}.{1}.{2}' -f $numbers[0],$numbers[1],$numbers[2])
if ($parsed.Groups['build'].Success) { $releaseLabel += '.' + $numbers[3] }
$releaseLabel += $parsed.Groups['suffix'].Value
$releaseTag = 'v' + $releaseLabel

function Replace-ExactlyOne([string]$text, [string]$pattern, [string]$replacement, [string]$label) {
    $hits = [regex]::Matches($text, $pattern)
    if ($hits.Count -ne 1) {
        throw "Expected exactly one $label entry in release metadata; found $($hits.Count). Source format may have changed."
    }
    $value = $hits[0].Groups['value']
    return $text.Substring(0, $value.Index) + $replacement +
        $text.Substring($value.Index + $value.Length)
}
function Change-Macro([string]$text, [string]$name, [string]$value) {
    $pattern = '(?m)^[ \t]*#define[ \t]+' + [regex]::Escape($name) + '[ \t]+(?<value>[^ \t\r\n]+)'
    return Replace-ExactlyOne $text $pattern $value $name
}
function Assert-GitVersionFilesClean([string]$repoRoot) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { return }
    $gitDir = & git -C $repoRoot rev-parse --is-inside-work-tree 2>$null
    if ($LASTEXITCODE -ne 0 -or "$gitDir".Trim() -ne 'true') { return }
    foreach ($statusArgs in @(
        @('diff','--quiet','--','spellcross-map-edit-main/source/app_version.h','spellcross-map-edit-main/source/resources.rc'),
        @('diff','--cached','--quiet','--','spellcross-map-edit-main/source/app_version.h','spellcross-map-edit-main/source/resources.rc')
    )) {
        & git -C $repoRoot @statusArgs
        if ($LASTEXITCODE -ne 0) {
            throw 'app_version.h or resources.rc contains local changes. Commit/stash those files before releasing; refusing to overwrite them.'
        }
    }
}
Assert-GitVersionFilesClean $root

# Use a single-byte reversible view of the files. This preserves every byte
# outside the ASCII version tokens even if the RC contains CP1250 or UTF-8
# comments. Both files are restored from their original byte arrays.
$byteEncoding = [System.Text.Encoding]::GetEncoding(28591)
$originalHeader = [System.IO.File]::ReadAllBytes($headerPath)
$originalResources = [System.IO.File]::ReadAllBytes($resourcesPath)
$header = $byteEncoding.GetString($originalHeader)
$resources = $byteEncoding.GetString($originalResources)

$metadata = [ordered]@{
    'SPELLCROSS_VERSION_MAJOR' = [string]$numbers[0]
    'SPELLCROSS_VERSION_MINOR' = [string]$numbers[1]
    'SPELLCROSS_VERSION_PATCH' = [string]$numbers[2]
    'SPELLCROSS_VERSION_BUILD' = [string]$numbers[3]
    'SPELLCROSS_VERSION_COMMA' = ($numbers -join ',')
    'SPELLCROSS_VERSION_STRING' = ('"' + $numericVersion + '"')
    'SPELLCROSS_VERSION_WSTRING' = ('L"' + $numericVersion + '"')
    'SPELLCROSS_RELEASE_LABEL' = ('"' + $releaseLabel + '"')
    'SPELLCROSS_RELEASE_TAG' = ('"' + $releaseTag + '"')
}
foreach ($name in $metadata.Keys) {
    $header = Change-Macro $header $name $metadata[$name]
}
foreach ($name in @('FileVersion','ProductVersion')) {
    $regex = '(?m)^[ \t]*VALUE[ \t]+"' + $name + '"[ \t]*,[ \t]*"(?<value>[0-9]+(?:\.[0-9]+){3})\\0"'
    $resources = Replace-ExactlyOne $resources $regex $numericVersion $name
}

$changed = $false
$previousBuildOverride = [Environment]::GetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', 'Process')
try {
    $changed = $true  # Even a partial write must be repaired by finally.
    [System.IO.File]::WriteAllBytes($headerPath, $byteEncoding.GetBytes($header))
    [System.IO.File]::WriteAllBytes($resourcesPath, $byteEncoding.GetBytes($resources))

    # Verify the exact metadata we handed to the ordinary release packager.
    foreach ($name in $metadata.Keys) {
        $pattern = '(?m)^[ \t]*#define[ \t]+' + [regex]::Escape($name) +
            '[ \t]+(?<value>[^ \t\r\n]+)'
        $result = [regex]::Match($byteEncoding.GetString([IO.File]::ReadAllBytes($headerPath)), $pattern)
        if (-not $result.Success -or $result.Groups['value'].Value -ne $metadata[$name]) {
            throw "Could not persist expected $name value."
        }
    }
    $actualResources = $byteEncoding.GetString([IO.File]::ReadAllBytes($resourcesPath))
    foreach ($name in @('FileVersion','ProductVersion')) {
        $regex = '(?m)^[ \t]*VALUE[ \t]+"' + $name + '"[ \t]*,[ \t]*"(?<value>[0-9]+(?:\.[0-9]+){3})\\0"'
        $result = [regex]::Match($actualResources, $regex)
        if (-not $result.Success -or $result.Groups['value'].Value -ne $numericVersion) {
            throw "Could not persist expected resource $name value."
        }
    }
    Write-Host "Release: $releaseTag | Windows FileVersion/ProductVersion: $numericVersion" -ForegroundColor Cyan
    if ($ValidateOnly) {
        Write-Host 'Version metadata validation passed; no compilation requested.' -ForegroundColor Green
    }
    else {
        # Force a clean rebuild; incremental builds can keep old resources,
        # especially when the checked-in source version is restored afterward.
        $buildParameters = @{
            Rebuild = $true
            NoVersionPrompt = $true
        }
        foreach ($key in @('WxRoot','OutputDirectory','MovieSource','NoZip','NoSfx','WinRARPath','PruneOldBuilds')) {
            if ($PSBoundParameters.ContainsKey($key)) {
                $buildParameters[$key] = $PSBoundParameters[$key]
            }
        }
        [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $releaseLabel, 'Process')
        & (Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1') @buildParameters
        Write-Host "Release packages assembled for $releaseTag." -ForegroundColor Green
    }
}
finally {
    # Never leave main dirty merely because the release was compiled. This is
    # also executed on normal build/package errors or when -ValidateOnly is used.
    [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $previousBuildOverride, 'Process')
    if ($changed) {
        [System.IO.File]::WriteAllBytes($headerPath, $originalHeader)
        [System.IO.File]::WriteAllBytes($resourcesPath, $originalResources)
        Write-Host 'Original app_version.h and resources.rc restored.' -ForegroundColor DarkGray
    }
}
