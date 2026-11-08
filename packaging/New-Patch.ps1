# Delta packager for two portable release staging folders (Windows PowerShell 5.1).
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PreviousDirectory,
    [Parameter(Mandatory=$true)][string]$CurrentDirectory,
    [Parameter(Mandatory=$true)][string]$FromVersion,
    [Parameter(Mandatory=$true)][string]$ToVersion,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Assert-Version([string]$v) {
    if ($v -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?$') { throw "Invalid version: $v" }
}
function Is-ManagedPath([string]$p) {
    if ($p -match '(?i)(^|[\\/])(\.\.|config\.ini|game_data|save|saves|dlc|temp|updates|\.spellcross-update[^\\/]*)($|[\\/])') { return $false }
    if ($p -match '^[A-Za-z]:|^[/\\]' -or $p.Contains('..')) { return $false }
    return ($p -match '^(Spellcross\.exe|Editor[/\\].+|data[/\\](?!imports[/\\]).+|locales[/\\].+|symbols[/\\].+|third-party-licenses[/\\].+|setup[/\\].+|README-FIRST\.txt|BUILD-INFO\.txt|LICENSE-[^/\\]+\.txt|[^/\\]+\.dll)$')
}
function Get-Inventory([string]$root) {
    $map = @{}
    foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/').Replace('\','/')
        if (-not (Is-ManagedPath $relative)) { continue }
        $map[$relative] = [pscustomobject]@{
            source = $file.FullName
            hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            length = [int64]$file.Length
        }
    }
    return $map
}
Assert-Version $FromVersion
Assert-Version $ToVersion
$oldDir = (Resolve-Path -LiteralPath $PreviousDirectory).Path.TrimEnd('\','/')
$newDir = (Resolve-Path -LiteralPath $CurrentDirectory).Path.TrimEnd('\','/')
if ($oldDir -eq $newDir) { throw 'Previous and current stages must differ.' }
if (-not (Test-Path -LiteralPath (Join-Path $newDir 'Spellcross.exe') -PathType Leaf)) { throw 'New stage has no Spellcross.exe.' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$old = Get-Inventory $oldDir
$new = Get-Inventory $newDir
$changed = New-Object System.Collections.ArrayList
$deleted = New-Object System.Collections.ArrayList
$work = Join-Path ([IO.Path]::GetTempPath()) ("spellcross-patch-build-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    foreach ($rel in @($new.Keys | Sort-Object)) {
        $item = $new[$rel]
        $previous = if ($old.ContainsKey($rel)) { $old[$rel] } else { $null }
        if ($null -ne $previous -and $previous.hash -eq $item.hash) { continue }
        $dst = Join-Path $work ($rel.Replace('/', [IO.Path]::DirectorySeparatorChar))
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $item.source -Destination $dst
        [void]$changed.Add([ordered]@{
            path = $rel; size = $item.length; sha256 = $item.hash
            old_sha256 = if ($previous) { $previous.hash } else { '' }
        })
    }
    foreach ($rel in @($old.Keys | Sort-Object)) {
        if (-not $new.ContainsKey($rel)) {
            [void]$deleted.Add([ordered]@{ path = $rel; old_sha256 = $old[$rel].hash })
        }
    }
    $archive = ('Spellcross_Patch_v{0}_to_v{1}.zip' -f $FromVersion,$ToVersion)
    $zip = Join-Path $OutputDirectory $archive
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    [IO.Compression.ZipFile]::CreateFromDirectory($work, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
    $manifest = [ordered]@{
        schema = 1; from_version = $FromVersion; to_version = $ToVersion
        archive = $archive
        archive_sha256 = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        files = @($changed.ToArray()); deleted = @($deleted.ToArray())
    }
    $jsonPath = Join-Path $OutputDirectory 'patch-manifest.json'
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
    Write-Host ('Patch {0} -> {1}: {2} changed files, {3} removals' -f $FromVersion,$ToVersion,$changed.Count,$deleted.Count)
    Write-Host ('Upload BOTH assets to GitHub Release v{0}: {1} and {2}' -f $ToVersion,$jsonPath,$zip)
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
}
