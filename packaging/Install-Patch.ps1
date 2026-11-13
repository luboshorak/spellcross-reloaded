# Spellcross Reloaded optional updater. Launched by Spellcross.exe AFTER player consent.
# Windows PowerShell 5.1; no third-party libraries. Never touches player saves or imported ISO data.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$InstallRoot,
    [Parameter(Mandatory=$true)][string]$CurrentVersion,
    [Parameter(Mandatory=$true)][string]$TargetVersion,
    [int]$WaitPid = 0,
    [string]$LocalPatchDirectory = '',
    [switch]$NoRestart,
    [switch]$NoPause
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-Version([string]$v) {
    if ($v -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?$') { throw "Invalid version: $v" }
}
function Assert-Hash([string]$hash) {
    if ($hash -notmatch '^[a-fA-F0-9]{64}$') { throw "Invalid SHA256 in patch manifest." }
}
function Assert-Path([string]$p) {
    if ($p -notmatch '^[\x20-\x7E]+$' -or $p.Contains('\') -or $p.Contains('..') -or $p.Contains(':') -or $p.StartsWith('/') -or $p.EndsWith('/')) {
        throw "Unsafe patch path: $p"
    }
    if ($p -match '(?i)(^|/)(config\.ini|game_data|save|saves|dlc|temp|updates|\.spellcross-update[^/]*)(/|$)') { throw "Protected path in patch: $p" }
    if ($p -notmatch '^(Spellcross\.exe|Editor/.+|data/(?!imports/).+|locales/.+|symbols/.+|third-party-licenses/.+|setup/.+|updater/Install-Patch\.ps1|README-FIRST\.txt|BUILD-INFO\.txt|LICENSE-[^/]+\.txt|[^/]+\.dll)$') { throw "Unmanaged patch path: $p" }
    foreach ($segment in $p.Split('/')) {
        if ($segment -in @('.','..','') -or $segment.EndsWith('.') -or $segment.EndsWith(' ')) { throw "Unsafe filename: $p" }
    }
}
function Safe-Destination([string]$root, [string]$relative) {
    Assert-Path $relative
    $parts = $relative.Split('/')
    $candidate = $root
    foreach ($part in $parts) {
        $candidate = Join-Path $candidate $part
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to patch through a symlink/junction: $candidate"
            }
        }
    }
    return $candidate
}
function Verify-Existing([string]$path, [string]$expectedHash) {
    if (-not $expectedHash) {
        if (Test-Path -LiteralPath $path) { throw "Unexpected existing file; refusing to overwrite: $path" }
        return
    }
    Assert-Hash $expectedHash
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Installed file missing: $path" }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actual -ne $expectedHash) { throw "Installed file has been modified; refusing overwrite: $path" }
}
function Download-File([string]$url, [string]$dest) {
    if (-not $url.StartsWith('https://github.com/luboshorak/spellcross-reloaded/releases/download/')) { throw "Unsafe release asset URL." }
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $dest -Headers @{ 'User-Agent' = 'SpellcrossReloaded-Patcher' }
}
function Get-PatchSteps([string]$from, [string]$to, [string]$localDir, [string]$work) {
    $all = @{}
    if ($localDir) {
        $manifestFile = Join-Path $localDir 'patch-manifest.json'
        $manifest = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json
        $all[[string]$manifest.to_version] = [pscustomobject]@{ manifest=$manifest; source=$localDir; release=$null }
    } else {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $api = 'https://api.github.com/repos/luboshorak/spellcross-reloaded/releases?per_page=100'
        $releases = @(Invoke-RestMethod -Uri $api -Headers @{
            'User-Agent' = 'SpellcrossReloaded-Patcher'; 'Accept'='application/vnd.github+json'
        })
        foreach ($release in $releases) {
            if ($release.draft -or -not ([string]$release.tag_name -match '^v\d+\.\d+\.\d+(?:\.\d+)?(?:-(?:RC|beta)\d+)?$')) { continue }
            $asset = @($release.assets | Where-Object { $_.name -eq 'patch-manifest.json' } | Select-Object -First 1)
            if ($asset.Count -ne 1) { continue }
            $path = Join-Path $work ('manifest-' + $release.id + '.json')
            Download-File ([string]$asset[0].browser_download_url) $path
            $manifest = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
            $v = ([string]$release.tag_name).Substring(1)
            if ([string]$manifest.to_version -ne $v) { throw "Manifest/release version mismatch: $v" }
            $all[$v] = [pscustomobject]@{ manifest=$manifest; source=''; release=$release }
        }
    }
    $steps = New-Object System.Collections.ArrayList
    $cursor = $to
    $visited = @{}
    while ($cursor -ne $from) {
        Assert-Version $cursor
        if ($visited.ContainsKey($cursor) -or $steps.Count -ge 50) { throw 'Circular or overly long patch chain.' }
        $visited[$cursor] = $true
        if (-not $all.ContainsKey($cursor)) {
            throw "No patch chain found for $from -> $to (missing step ending at $cursor). Download a full build."
        }
        $step = $all[$cursor]
        $m = $step.manifest
        if ([int]$m.schema -ne 1 -or [string]$m.to_version -ne $cursor) { throw 'Unsupported or inconsistent patch manifest.' }
        Assert-Version ([string]$m.from_version)
        if ([string]$m.from_version -eq $cursor) { throw 'Patch cannot target its own version.' }
        [void]$steps.Add($step)
        $cursor = [string]$m.from_version
    }
    $result = @($steps.ToArray())
    [array]::Reverse($result)
    return $result
}

$lockStream = $null
$work = $null
try {
    Assert-Version $CurrentVersion
    Assert-Version $TargetVersion
    $root = (Resolve-Path -LiteralPath $InstallRoot).Path.TrimEnd('\','/')
    $exe = Join-Path $root 'Spellcross.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Spellcross.exe not found in selected installation.' }
    if ($CurrentVersion -eq $TargetVersion) { Write-Host 'Already up to date.'; exit 0 }
    if ($WaitPid -gt 0) {
        Write-Host 'Waiting for Spellcross.exe to close...'
        try { Wait-Process -Id $WaitPid -Timeout 45 -ErrorAction Stop }
        catch {
            if (Get-Process -Id $WaitPid -ErrorAction SilentlyContinue) { throw 'Game is still running; update cancelled.' }
        }
    }
    $lockPath = Join-Path $root '.spellcross-update.lock'
    $lockStream = [IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $work = Join-Path $root ('.spellcross-update-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    $staged = Join-Path $work 'new'
    $backup = Join-Path $work 'backup'
    New-Item -ItemType Directory -Force -Path $staged,$backup | Out-Null
    Write-Host ('Preparing update {0} -> {1}' -f $CurrentVersion,$TargetVersion)
    $steps = @(Get-PatchSteps $CurrentVersion $TargetVersion $LocalPatchDirectory $work)
    $operations = @{}
    foreach ($step in $steps) {
        $m = $step.manifest
        Assert-Hash ([string]$m.archive_sha256)
        $name = [string]$m.archive
        if ($name -notmatch '^Spellcross_Patch_v[\w.-]+_to_v[\w.-]+\.zip$' -or $name.Contains('..')) { throw 'Invalid patch archive name.' }
        $zipPath = Join-Path $work $name
        if ($step.source) {
            Copy-Item -LiteralPath (Join-Path $step.source $name) -Destination $zipPath
        } else {
            $asset = @($step.release.assets | Where-Object { $_.name -eq $name } | Select-Object -First 1)
            if ($asset.Count -ne 1) { throw "Patch ZIP missing from release: $name" }
            Download-File ([string]$asset[0].browser_download_url) $zipPath
        }
        if ((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash -ne [string]$m.archive_sha256) {
            throw "Patch ZIP SHA256 mismatch: $name"
        }
        $zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
        try {
            $expected = @{}
            foreach ($entry in @($m.files)) {
                $rel = [string]$entry.path
                Assert-Path $rel
                if ($expected.ContainsKey($rel)) { throw "Duplicate manifest path: $rel" }
                $expected[$rel] = $true
                $oldPath = Safe-Destination $root $rel
                if ($operations.ContainsKey($rel) -and $operations[$rel] -eq 'write') {
                    $oldPath = Safe-Destination $staged $rel
                } elseif ($operations.ContainsKey($rel) -and $operations[$rel] -eq 'delete') {
                    $oldPath = Safe-Destination $staged $rel
                }
                Verify-Existing $oldPath ([string]$entry.old_sha256)
                Assert-Hash ([string]$entry.sha256)
                # .NET Framework 4.x may store nested entry names with '\\'
                # while newer runtimes use '/'. The signed manifest always
                # uses portable '/' paths; normalize before matching.
                $archiveMatches = @($zip.Entries | Where-Object {
                    $_.FullName.Replace('\','/') -eq $rel
                })
                if ($archiveMatches.Count -ne 1) {
                    throw "Missing or duplicate ZIP entry: $rel"
                }
                $archiveEntry = $archiveMatches[0]
                if ($archiveEntry.Length -ne [int64]$entry.size -or $archiveEntry.Length -gt 1073741824) {
                    throw "Missing or invalid ZIP entry: $rel"
                }
                $out = Safe-Destination $staged $rel
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null
                $input = $archiveEntry.Open()
                $output = [IO.File]::Create($out)
                try { $input.CopyTo($output) } finally { $input.Dispose(); $output.Dispose() }
                if ((Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash -ne [string]$entry.sha256) {
                    throw "Patch file SHA256 mismatch: $rel"
                }
                $operations[$rel] = 'write'
            }
            $seenArchiveEntries = @{}
            foreach ($entry in $zip.Entries) {
                $zipRelative = ([string]$entry.FullName).Replace('\','/')
                if ($entry.Name) {
                    if ($seenArchiveEntries.ContainsKey($zipRelative)) {
                        throw "Duplicate file in ZIP: $zipRelative"
                    }
                    $seenArchiveEntries[$zipRelative] = $true
                }
                if ($entry.Name -and -not $expected.ContainsKey($zipRelative)) {
                    throw "Unexpected file in ZIP: $zipRelative"
                }
            }
            foreach ($item in @($m.deleted)) {
                $rel = [string]$item.path
                Assert-Path $rel
                if ($expected.ContainsKey($rel)) { throw "Conflicting write/delete: $rel" }
                $oldPath = if ($operations.ContainsKey($rel) -and $operations[$rel] -eq 'write') {
                    Safe-Destination $staged $rel
                } else {
                    Safe-Destination $root $rel
                }
                Verify-Existing $oldPath ([string]$item.old_sha256)
                $stagedPath = Safe-Destination $staged $rel
                if (Test-Path -LiteralPath $stagedPath -PathType Leaf) { Remove-Item -LiteralPath $stagedPath -Force }
                $operations[$rel] = 'delete'
            }
        } finally { $zip.Dispose() }
    }
    Write-Host ('Validated all downloads and {0} file operations. Installing...' -f $operations.Count)
    $applied = New-Object System.Collections.ArrayList
    try {
        foreach ($rel in @($operations.Keys | Sort-Object)) {
            $target = Safe-Destination $root $rel
            $oldCopy = Safe-Destination $backup $rel
            if (Test-Path -LiteralPath $target -PathType Leaf) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $oldCopy) | Out-Null
                Move-Item -LiteralPath $target -Destination $oldCopy
            }
            [void]$applied.Add($rel)
            if ($operations[$rel] -eq 'write') {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
                Move-Item -LiteralPath (Safe-Destination $staged $rel) -Destination $target
            }
        }
    } catch {
        $cause = $_
        Write-Warning 'Patch failed during installation; restoring original files.'
        for ($i=$applied.Count-1; $i -ge 0; $i--) {
            $rel = [string]$applied[$i]
            $target = Safe-Destination $root $rel
            $oldCopy = Safe-Destination $backup $rel
            if (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force }
            if (Test-Path -LiteralPath $oldCopy -PathType Leaf) {
                Move-Item -LiteralPath $oldCopy -Destination $target
            }
        }
        throw $cause
    }
    Write-Host ('Spellcross Reloaded updated to v{0}.' -f $TargetVersion) -ForegroundColor Green
    if (-not $NoRestart) {
        try { Start-Process -FilePath $exe -WorkingDirectory $root -ArgumentList '--no-update-check' }
        catch { Write-Warning ('Updated successfully, but automatic restart failed: ' + $_.Exception.Message) }
    }
} catch {
    Write-Host ('Update cancelled: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Your existing installation and player data have been preserved.'
    if (-not $NoPause) { Read-Host 'Press Enter to close the updater' | Out-Null }
    exit 1
} finally {
    if ($lockStream) { $lockStream.Dispose() }
    if ($work -and (Test-Path -LiteralPath $work)) {
        try { Remove-Item -LiteralPath $work -Recurse -Force } catch { Write-Warning "Could not remove update scratch folder: $work" }
    }
    if ($lockStream) { try { Remove-Item -LiteralPath $lockPath -Force } catch {} }
}

# Explicit success status (do not inherit $LASTEXITCODE from another process).
exit 0
