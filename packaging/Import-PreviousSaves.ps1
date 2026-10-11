# Import only user-owned save games from an older Spellcross Reloaded install.
# Usage from a combined release:
#   .\Import-PreviousSaves.ps1 -PreviousFolder "D:\Games\Spellcross-v0.8.0"
# Never overwrites current saves; does not copy game archives or configuration.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PreviousFolder,
    [string]$TargetFolder = $PSScriptRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$from = (Resolve-Path -LiteralPath $PreviousFolder).Path
$to = (Resolve-Path -LiteralPath $TargetFolder).Path
if (-not (Test-Path -LiteralPath (Join-Path $from 'Spellcross.exe') -PathType Leaf)) {
    throw "PreviousFolder must contain the OLD Spellcross.exe: $from"
}
if (-not (Test-Path -LiteralPath (Join-Path $to 'Spellcross.exe') -PathType Leaf)) {
    throw "TargetFolder must contain the NEW Spellcross.exe: $to"
}
if ([string]::Equals($from.TrimEnd('\'), $to.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Source and destination installations are identical.'
}
$copied = 0
$skipped = 0

function Copy-Save([string]$Source, [string]$Destination) {
    $folder = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    if (Test-Path -LiteralPath $Destination) {
        $script:skipped++
        return
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -ErrorAction Stop
    $script:copied++
}
$oldStrategic = Join-Path $from 'save\strategic'
if (Test-Path -LiteralPath $oldStrategic -PathType Container) {
    foreach ($file in (Get-ChildItem -LiteralPath $oldStrategic -File -Recurse)) {
        if ($file.Extension -notin @('.json','.sav','.scsave')) { continue }
        $relative = $file.FullName.Substring($oldStrategic.Length).TrimStart('\')
        $dest = Join-Path (Join-Path $to 'save\strategic') $relative
        Copy-Save $file.FullName $dest
    }
}
$oldTemp = Join-Path $from 'temp'
if (Test-Path -LiteralPath $oldTemp -PathType Container) {
    foreach ($file in (Get-ChildItem -LiteralPath $oldTemp -Filter '*.scsave' -File)) {
        Copy-Save $file.FullName (Join-Path (Join-Path $to 'temp') $file.Name)
    }
}
Write-Host "Imported $copied save(s), skipped $skipped existing file(s)." -ForegroundColor Green
Write-Host "No game archives or configs were copied. Your original installation was not modified."
