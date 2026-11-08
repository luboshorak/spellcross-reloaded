# Spellcross Reloaded – one-command release version setup.
# Usage: .\packaging\Release.ps1 [-Version 0.9.6] [-NoSfx] [-WxRoot ...]
# Windows PowerShell 5.1+. Version metadata is committed AFTER successful
# compilation, then packages are stamped with the commit SHA and pushed.
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
    [switch]$ValidateOnly,
    [switch]$NoPush,
    [switch]$AllowNonMain,
    [string]$PreviousStageDirectory = '',
    [string]$PreviousVersion = ''
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

# The chosen version will be committed to source control after compilation.
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
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        if ($ValidateOnly) { return }
        throw 'git.exe is required to commit and push a release version.'
    }
    $gitDir = & git -C $repoRoot rev-parse --is-inside-work-tree 2>$null
    if ($LASTEXITCODE -ne 0 -or "$gitDir".Trim() -ne 'true') {
        if ($ValidateOnly) { return }
        throw 'Run the release script from a valid Git working tree.'
    }
    # A dirty submodule working tree does not change the tracked superproject
    # commit. We rebuild the editor from its PINNED commit in a temporary
    # worktree below, never from the player's locally modified files.
    # This still detects a changed/staged gitlink and all other project edits.
    $status = @(& git -C $repoRoot status --porcelain --ignore-submodules=dirty --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw 'Could not read Git worktree status.' }
    if ($status.Count -gt 0) {
        throw ('Git worktree must be clean before release: ' + ($status -join [Environment]::NewLine))
    }
    if ($ValidateOnly) { return }
    $branch = (& git -C $repoRoot branch --show-current).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $branch) { throw 'Could not identify current Git branch.' }
    if ($branch -ne 'main' -and -not ($NoPush -and $AllowNonMain)) {
        throw "Release expects main. Current branch: $branch. Test a feature branch only with -NoPush -AllowNonMain."
    }
    if ($AllowNonMain -and -not $NoPush) { throw '-AllowNonMain requires -NoPush.' }
    if (-not $NoPush) {
        & git -C $repoRoot fetch --quiet origin main
        if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch origin/main; refusing to publish an unverified release.' }
        $localCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
        $remoteCommit = (& git -C $repoRoot rev-parse refs/remotes/origin/main).Trim()
        if ($localCommit -ne $remoteCommit) {
            throw 'Local main differs from origin/main. Run git pull --ff-only before releasing.'
        }
    }
}
function Invoke-ReleaseGit([string[]]$gitArguments) {
    & git -C $root @gitArguments
    if ($LASTEXITCODE -ne 0) {
        throw ("Git command failed (exit " + $LASTEXITCODE + "): git " + ($gitArguments -join ' '))
    }
}
function New-PinnedEditorCheckout {
    $editorRepository = Join-Path $root 'external\map-editor'
    $gitlink = @(& git -C $root ls-files --stage -- 'external/map-editor')
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read pinned map-editor Git submodule.' }
    if ($gitlink.Count -eq 0) {
        # Minimal Git fixtures may not include a submodule.
        return ''
    }
    if ($gitlink.Count -ne 1 -or
        $gitlink[0] -notmatch '^160000 ([0-9a-f]{40}) 0\s+external/map-editor$') {
        throw 'Invalid map-editor gitlink; refusing release.'
    }
    $pinnedCommit = $Matches[1]
    if (-not (Test-Path -LiteralPath $editorRepository -PathType Container)) {
        throw 'Editor submodule is missing. Run git submodule update --init --recursive.'
    }
    $editorHead = @(& git -C $editorRepository rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $editorHead.Count -ne 1 -or
        $editorHead[0].Trim() -ne $pinnedCommit) {
        throw 'Editor checkout differs from its pinned commit. Preserve your local edits before updating it.'
    }
    $dirty = @(& git -C $editorRepository status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect editor submodule changes.' }
    if ($dirty.Count -eq 0) { return '' }
    $checkout = Join-Path ([IO.Path]::GetTempPath()) ('spellcross-pinned-editor-' + [guid]::NewGuid().ToString('N'))
    Write-Warning 'Editor submodule is modified. Building from a clean temporary checkout of the pinned commit; local changes are untouched.'
    & git -C $editorRepository worktree add --detach $checkout $pinnedCommit
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the pinned editor checkout; local editor files were not changed.'
    }
    return $checkout
}
Assert-GitVersionFilesClean $root
if (-not $ValidateOnly) {
    $existingTag = @(& git -C $root tag --list $releaseTag)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect existing release tags.' }
    if ($existingTag.Count -gt 0) { throw "Release tag $releaseTag already exists locally." }
    if (-not $NoPush) {
        $remoteTag = @(& git -C $root ls-remote --tags origin ("refs/tags/" + $releaseTag))
        if ($LASTEXITCODE -ne 0) { throw 'Could not verify remote release tags.' }
        if ($remoteTag.Count -gt 0) { throw "Release tag $releaseTag already exists on origin." }
    }
    $currentText = Get-Content -LiteralPath $headerPath -Raw
    $currentLabel = [regex]::Match($currentText, '(?m)^[ \t]*#define[ \t]+SPELLCROSS_RELEASE_LABEL[ \t]+"([^"]+)"')
    if (-not $currentLabel.Success) { throw 'Could not find current release label.' }
    if ($currentLabel.Groups[1].Value -eq $releaseLabel) {
        throw "Requested version $releaseLabel is already checked into the source. Choose a new version."
    }
    $currentNumericMatch = [regex]::Match($currentText,
        '(?m)^[ \t]*#define[ \t]+SPELLCROSS_VERSION_STRING[ \t]+"([0-9]+(?:\.[0-9]+){3})"')
    if (-not $currentNumericMatch.Success) { throw 'Could not read current numeric version.' }
    if ([version]$numericVersion -lt [version]$currentNumericMatch.Groups[1].Value) {
        throw "Refusing to downgrade from $($currentNumericMatch.Groups[1].Value) to $numericVersion."
    }
    if ($NoZip) { throw 'Automated release always creates a full ZIP. -NoZip is only for the legacy packager.' }
}

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
$commitCreated = $false
$cleanEditorWorktree = ''
$addAttempted = $false
$previousHead = ''
if (-not $ValidateOnly) {
    $previousHead = (& git -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $previousHead) { throw 'Cannot resolve Git HEAD.' }
}
$previousBuildOverride = [Environment]::GetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', 'Process')
try {
    $changed = $true
    [System.IO.File]::WriteAllBytes($headerPath, $byteEncoding.GetBytes($header))
    [System.IO.File]::WriteAllBytes($resourcesPath, $byteEncoding.GetBytes($resources))
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
        Write-Host 'Version metadata validation passed; source files will be restored.' -ForegroundColor Green
    } else {
        # 1. Compile and verify PE version before any Git commit.
        $cleanEditorWorktree = New-PinnedEditorCheckout
        $buildParameters = @{ Rebuild = $true; NoVersionPrompt = $true; BuildOnly = $true }
        if ($cleanEditorWorktree) {
            $buildParameters['EditorRoot'] = $cleanEditorWorktree
        }
        foreach ($key in @('WxRoot','MovieSource','WinRARPath','NoSfx')) {
            if ($PSBoundParameters.ContainsKey($key)) { $buildParameters[$key] = $PSBoundParameters[$key] }
        }
        [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $releaseLabel, 'Process')
        Write-Host "Phase 1/4: compiling $releaseTag ..." -ForegroundColor Cyan
        & (Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1') @buildParameters

        # 2. Commit only version header and Windows resource metadata.
        Write-Host 'Phase 2/4: committing release version ...' -ForegroundColor Cyan
        $addAttempted = $true
        Invoke-ReleaseGit @('add', '--',
            'spellcross-map-edit-main/source/app_version.h',
            'spellcross-map-edit-main/source/resources.rc')
        Invoke-ReleaseGit @('commit', '-m', "chore(release): set version $releaseTag")
        $commitCreated = $true
        $releaseCommit = (& git -C $root rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $releaseCommit) {
            throw 'Committed release version but could not resolve HEAD.'
        }

        # 3. Package after commit: BUILD-INFO.txt now has the exact SHA.
        Write-Host "Phase 3/4: packaging binaries from $releaseCommit ..." -ForegroundColor Cyan
        $packageParameters = @{ SkipBuild = $true; NoVersionPrompt = $true }
        if ($cleanEditorWorktree) {
            $packageParameters['EditorRoot'] = $cleanEditorWorktree
        }
        foreach ($key in @('WxRoot','OutputDirectory','MovieSource','NoZip','NoSfx','WinRARPath','PruneOldBuilds','PreviousStageDirectory','PreviousVersion')) {
            if ($PSBoundParameters.ContainsKey($key)) { $packageParameters[$key] = $PSBoundParameters[$key] }
        }
        & (Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1') @packageParameters

        # 4. Local tag and an atomic, non-forced push of branch + tag.
        Write-Host 'Phase 4/4: tagging validated release ...' -ForegroundColor Cyan
        Invoke-ReleaseGit @('tag', '-a', $releaseTag, '-m', "Spellcross Reloaded $releaseTag")
        if ($NoPush) {
            Write-Warning "Release committed and tagged LOCALLY ($releaseTag @ $releaseCommit); -NoPush selected."
        } else {
            Write-Host "Pushing release commit and $releaseTag to origin/main atomically ..." -ForegroundColor Cyan
            Invoke-ReleaseGit @('push', '--atomic', 'origin', 'HEAD:refs/heads/main',
                ("refs/tags/" + $releaseTag + ":refs/tags/" + $releaseTag))
            Write-Host "Release source pushed: $releaseTag @ $releaseCommit" -ForegroundColor Green
        }
        Write-Host 'Packages created. GitHub Release assets are NOT uploaded automatically.' -ForegroundColor Green
    }
}
finally {
    [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $previousBuildOverride, 'Process')
    if ($cleanEditorWorktree) {
        $editorRepository = Join-Path $root 'external\map-editor'
        & git -C $editorRepository worktree remove --force $cleanEditorWorktree
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Could not remove temporary pinned editor worktree: $cleanEditorWorktree"
        }
    }
    if ($changed -and -not $commitCreated -and $previousHead) {
        # If a post-commit hook returned an error after Git moved HEAD,
        # never revert version bytes against a newly recorded commit.
        $currentHead = (& git -C $root rev-parse HEAD).Trim()
        if ($LASTEXITCODE -eq 0 -and $currentHead -ne $previousHead) {
            $commitCreated = $true
            Write-Warning 'Git HEAD changed during release; leaving committed metadata untouched.'
        }
    }
    if ($changed -and -not $commitCreated) {
        if ($addAttempted) {
            & git -C $root restore --staged -- 'spellcross-map-edit-main/source/app_version.h' 'spellcross-map-edit-main/source/resources.rc'
            if ($LASTEXITCODE -ne 0) { Write-Warning 'Could not unstage release metadata; inspect git status.' }
        }
        [System.IO.File]::WriteAllBytes($headerPath, $originalHeader)
        [System.IO.File]::WriteAllBytes($resourcesPath, $originalResources)
        Write-Host 'No version commit: original metadata restored.' -ForegroundColor Yellow
    } elseif ($commitCreated) {
        # After a successful commit even package/push failure MUST NOT erase
        # the metadata; both source and released executables stay consistent.
        Write-Host 'Version metadata remains permanently committed locally.' -ForegroundColor Green
    }
}
) {
        throw 'Invalid map-editor gitlink; refusing release.'
    }
    $pinnedCommit = $Matches[1]
    if (-not (Test-Path -LiteralPath $editorRepository -PathType Container)) {
        throw 'Editor submodule is missing. Run git submodule update --init --recursive.'
    }
    $editorHead = @(& git -C $editorRepository rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $editorHead.Count -ne 1 -or
        $editorHead[0].Trim() -ne $pinnedCommit) {
        throw 'Editor checkout is not at its pinned commit. Run git submodule update only after saving your editor changes.'
    }
    $dirty = @(& git -C $editorRepository status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect editor submodule changes.' }
    if ($dirty.Count -eq 0) { return '' }

    # Never reset, clean, stash or overwrite the developer's editor checkout.
    # A detached temporary worktree guarantees that release binaries represent
    # the commit actually referenced by the main Spellcross repository.
    $checkout = Join-Path ([IO.Path]::GetTempPath()) ('spellcross-pinned-editor-' + [guid]::NewGuid().ToString('N'))
    Write-Warning 'Local editor submodule is modified. Release will use a clean temporary checkout of its pinned commit; local editor changes are untouched.'
    & git -C $editorRepository worktree add --detach $checkout $pinnedCommit
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the pinned editor checkout. Preserve your local editor changes and inspect the Git error above.'
    }
    return $checkout
}
Assert-GitVersionFilesClean $root
if (-not $ValidateOnly) {
    $existingTag = @(& git -C $root tag --list $releaseTag)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect existing release tags.' }
    if ($existingTag.Count -gt 0) { throw "Release tag $releaseTag already exists locally." }
    if (-not $NoPush) {
        $remoteTag = @(& git -C $root ls-remote --tags origin ("refs/tags/" + $releaseTag))
        if ($LASTEXITCODE -ne 0) { throw 'Could not verify remote release tags.' }
        if ($remoteTag.Count -gt 0) { throw "Release tag $releaseTag already exists on origin." }
    }
    $currentText = Get-Content -LiteralPath $headerPath -Raw
    $currentLabel = [regex]::Match($currentText, '(?m)^[ \t]*#define[ \t]+SPELLCROSS_RELEASE_LABEL[ \t]+"([^"]+)"')
    if (-not $currentLabel.Success) { throw 'Could not find current release label.' }
    if ($currentLabel.Groups[1].Value -eq $releaseLabel) {
        throw "Requested version $releaseLabel is already checked into the source. Choose a new version."
    }
    $currentNumericMatch = [regex]::Match($currentText,
        '(?m)^[ \t]*#define[ \t]+SPELLCROSS_VERSION_STRING[ \t]+"([0-9]+(?:\.[0-9]+){3})"')
    if (-not $currentNumericMatch.Success) { throw 'Could not read current numeric version.' }
    if ([version]$numericVersion -lt [version]$currentNumericMatch.Groups[1].Value) {
        throw "Refusing to downgrade from $($currentNumericMatch.Groups[1].Value) to $numericVersion."
    }
    if ($NoZip) { throw 'Automated release always creates a full ZIP. -NoZip is only for the legacy packager.' }
}

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
$commitCreated = $false
$addAttempted = $false
$previousHead = ''
if (-not $ValidateOnly) {
    $previousHead = (& git -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $previousHead) { throw 'Cannot resolve Git HEAD.' }
}
$previousBuildOverride = [Environment]::GetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', 'Process')
try {
    $changed = $true
    [System.IO.File]::WriteAllBytes($headerPath, $byteEncoding.GetBytes($header))
    [System.IO.File]::WriteAllBytes($resourcesPath, $byteEncoding.GetBytes($resources))
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
        Write-Host 'Version metadata validation passed; source files will be restored.' -ForegroundColor Green
    } else {
        # 1. Compile and verify PE version before any Git commit.
        $buildParameters = @{ Rebuild = $true; NoVersionPrompt = $true; BuildOnly = $true }
        foreach ($key in @('WxRoot','MovieSource','WinRARPath','NoSfx')) {
            if ($PSBoundParameters.ContainsKey($key)) { $buildParameters[$key] = $PSBoundParameters[$key] }
        }
        [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $releaseLabel, 'Process')
        Write-Host "Phase 1/4: compiling $releaseTag ..." -ForegroundColor Cyan
        & (Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1') @buildParameters

        # 2. Commit only version header and Windows resource metadata.
        Write-Host 'Phase 2/4: committing release version ...' -ForegroundColor Cyan
        $addAttempted = $true
        Invoke-ReleaseGit @('add', '--',
            'spellcross-map-edit-main/source/app_version.h',
            'spellcross-map-edit-main/source/resources.rc')
        Invoke-ReleaseGit @('commit', '-m', "chore(release): set version $releaseTag")
        $commitCreated = $true
        $releaseCommit = (& git -C $root rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $releaseCommit) {
            throw 'Committed release version but could not resolve HEAD.'
        }

        # 3. Package after commit: BUILD-INFO.txt now has the exact SHA.
        Write-Host "Phase 3/4: packaging binaries from $releaseCommit ..." -ForegroundColor Cyan
        $packageParameters = @{ SkipBuild = $true; NoVersionPrompt = $true }
        foreach ($key in @('WxRoot','OutputDirectory','MovieSource','NoZip','NoSfx','WinRARPath','PruneOldBuilds')) {
            if ($PSBoundParameters.ContainsKey($key)) { $packageParameters[$key] = $PSBoundParameters[$key] }
        }
        & (Join-Path $PSScriptRoot 'Build-CombinedRelease.ps1') @packageParameters

        # 4. Local tag and an atomic, non-forced push of branch + tag.
        Write-Host 'Phase 4/4: tagging validated release ...' -ForegroundColor Cyan
        Invoke-ReleaseGit @('tag', '-a', $releaseTag, '-m', "Spellcross Reloaded $releaseTag")
        if ($NoPush) {
            Write-Warning "Release committed and tagged LOCALLY ($releaseTag @ $releaseCommit); -NoPush selected."
        } else {
            Write-Host "Pushing release commit and $releaseTag to origin/main atomically ..." -ForegroundColor Cyan
            Invoke-ReleaseGit @('push', '--atomic', 'origin', 'HEAD:refs/heads/main',
                ("refs/tags/" + $releaseTag + ":refs/tags/" + $releaseTag))
            Write-Host "Release source pushed: $releaseTag @ $releaseCommit" -ForegroundColor Green
        }
        Write-Host 'Packages created. GitHub Release assets are NOT uploaded automatically.' -ForegroundColor Green
    }
}
finally {
    [Environment]::SetEnvironmentVariable('SPELLCROSS_RELEASE_OVERRIDE', $previousBuildOverride, 'Process')
    if ($changed -and -not $commitCreated -and $previousHead) {
        # If a post-commit hook returned an error after Git moved HEAD,
        # never revert version bytes against a newly recorded commit.
        $currentHead = (& git -C $root rev-parse HEAD).Trim()
        if ($LASTEXITCODE -eq 0 -and $currentHead -ne $previousHead) {
            $commitCreated = $true
            Write-Warning 'Git HEAD changed during release; leaving committed metadata untouched.'
        }
    }
    if ($changed -and -not $commitCreated) {
        if ($addAttempted) {
            & git -C $root restore --staged -- 'spellcross-map-edit-main/source/app_version.h' 'spellcross-map-edit-main/source/resources.rc'
            if ($LASTEXITCODE -ne 0) { Write-Warning 'Could not unstage release metadata; inspect git status.' }
        }
        [System.IO.File]::WriteAllBytes($headerPath, $originalHeader)
        [System.IO.File]::WriteAllBytes($resourcesPath, $originalResources)
        Write-Host 'No version commit: original metadata restored.' -ForegroundColor Yellow
    } elseif ($commitCreated) {
        # After a successful commit even package/push failure MUST NOT erase
        # the metadata; both source and released executables stay consistent.
        Write-Host 'Version metadata remains permanently committed locally.' -ForegroundColor Green
    }
}
