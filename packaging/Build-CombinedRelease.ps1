# Spellcross Reloaded + original map editor: clean portable x64 release.
# Windows PowerShell 5.1+, MSVC Build Tools, wxWidgets/vcpkg x64.
# Usage: .\packaging\Build-CombinedRelease.ps1 [-SkipBuild] [-WxRoot <path>] [-NoSfx] [-WinRARPath <path>]
[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$Rebuild,
    [string]$WxRoot = '',
    [string]$OutputDirectory = '',
    [string]$MovieSource = '',
    [switch]$NoZip,
    [switch]$NoSfx,
    [string]$WinRARPath = '',
    [switch]$PruneOldBuilds
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$game = Join-Path $root 'spellcross-map-edit-main'
# Keep packaging synchronized with the app and Windows version metadata.
$versionHeader = Join-Path $game 'source\app_version.h'
if (-not (Test-Path -LiteralPath $versionHeader -PathType Leaf)) { throw "Version header not found: $versionHeader" }
$versionMatch = [regex]::Match((Get-Content -LiteralPath $versionHeader -Raw),
    '(?m)^\s*#define\s+SPELLCROSS_VERSION_STRING\s+"(\d+\.\d+\.\d+(?:\.\d+)?)"\s*$')
if (-not $versionMatch.Success) { throw "Cannot read SPELLCROSS_VERSION_STRING from $versionHeader" }
$releaseVersion = $versionMatch.Groups[1].Value
$editor = Join-Path $root 'external\map-editor'
$gameProject = Join-Path $game 'source\spellcross_map_editor.vcxproj'
$editorProject = Join-Path $editor 'source\spellcross_map_editor.vcxproj'
$gameExe = Join-Path $game 'builds\x64\Release\Spellcross.exe'
$editorExe = Join-Path $editor 'builds\x64\Release\spellcross_map_editor.exe'

function Assert-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing file: $Path" }
}
function Copy-Required([string]$From, [string]$To) {
    Assert-File $From
    Copy-Item -LiteralPath $From -Destination $To -Force
}
Assert-File $gameProject
Assert-File $editorProject
Assert-File (Join-Path $editor 'data\COMMON.fs')

# WinRAR is optional for regular ZIP builds, but when found we also emit an
# SFX installer. -NoSfx disables it; -WinRARPath overrides autodetection.
# SFX branding assets are version-independent and live in packaging\setup.
$winrarExe = $null
$sfxSetup = Join-Path $PSScriptRoot 'setup'
$sfxIcon = Join-Path $sfxSetup 'spellcross_reloaded.ico'
$sfxLogo = Join-Path $sfxSetup 'spellcross_sfx_logo.png'
$sfxLogoHd = Join-Path $sfxSetup 'spellcross_sfx_logo_hd.png'
if ($NoSfx -and $WinRARPath) {
    throw 'Choose either -NoSfx or -WinRARPath, not both.'
}
if (-not $NoSfx) {
    if ($WinRARPath) {
        Assert-File $WinRARPath
        $winrarExe = (Resolve-Path -LiteralPath $WinRARPath).Path
    } else {
        $candidates = @()
        $found = Get-Command 'WinRAR.exe' -ErrorAction SilentlyContinue
        if ($found) { $candidates += $found.Source }
        if ($env:ProgramFiles) {
            $candidates += (Join-Path $env:ProgramFiles 'WinRAR\WinRAR.exe')
        }
        if (${env:ProgramFiles(x86)}) {
            $candidates += (Join-Path ${env:ProgramFiles(x86)} 'WinRAR\WinRAR.exe')
        }
        foreach ($candidate in $candidates) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $winrarExe = (Resolve-Path -LiteralPath $candidate).Path
                break
            }
        }
    }
    if ($winrarExe) {
        foreach ($asset in @($sfxIcon, $sfxLogo, $sfxLogoHd)) { Assert-File $asset }
        # Use the GUI RAR SFX module, which understands Title, Path and Shortcut.
        Assert-File (Join-Path (Split-Path -Parent $winrarExe) 'Default.SFX')
        Write-Host "WinRAR SFX enabled: $winrarExe" -ForegroundColor Cyan
    } else {
        Write-Warning 'WinRAR.exe was not found: the ZIP will be created, but not an SFX EXE. Install WinRAR or use -WinRARPath to specify it.'
    }
}

$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
$dumpbin = $null
if (Test-Path -LiteralPath $vswhere) {
    $m = @(& $vswhere -latest -products '*' -find 'MSBuild\Current\Bin\MSBuild.exe')
    $d = @(& $vswhere -latest -products '*' -find 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe')
    if ($m.Count -gt 0) { $msbuild = $m[0] }
    if ($d.Count -gt 0) { $dumpbin = $d[0] }
}
if (-not $msbuild) {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { $msbuild = $cmd.Source }
}
if (-not $dumpbin) {
    $cmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($cmd) { $dumpbin = $cmd.Source }
}

if (-not $WxRoot) {
    foreach ($candidate in @(
        $env:wxwin,
        (Join-Path $HOME 'vcpkg\installed\x64-windows'),
        $(if ($env:VCPKG_ROOT) { Join-Path $env:VCPKG_ROOT 'installed\x64-windows' })
    )) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'include') -PathType Container)) {
            $WxRoot = $candidate
            break
        }
    }
}
if ($WxRoot) { $WxRoot = (Resolve-Path -LiteralPath $WxRoot).Path.TrimEnd('\') + '\' }

if ($SkipBuild -and $Rebuild) { throw 'Cannot combine -SkipBuild and -Rebuild.' }
if (-not $SkipBuild) {
    if (-not $msbuild) { throw 'MSBuild unavailable. Install Visual Studio C++ tools or use -SkipBuild.' }
    if (-not $WxRoot) { throw 'wxWidgets root unavailable. Supply -WxRoot path-to-vcpkg\installed\x64-windows.' }
    $buildTarget = if ($Rebuild) { '/t:Rebuild' } else { '/t:Build' }
    foreach ($project in @($gameProject, $editorProject)) {
        Write-Host "Building Release|x64 ($buildTarget): $project"
        & $msbuild $project '/m' $buildTarget '/p:Configuration=Release' '/p:Platform=x64' ("/p:wxwin=" + $WxRoot) '/v:minimal'
        if ($LASTEXITCODE -ne 0) { throw "MSBuild failed: $project" }
    }
}
Assert-File $gameExe
Assert-File $editorExe
# The package name and BUILD-INFO.txt must reflect the actual binary, not just
# the current source header. This catches stale Release builds after a version
# bump (especially when -SkipBuild is used).
$builtVersionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($gameExe)
$builtFileVersion = [string]$builtVersionInfo.FileVersion
$builtProductVersion = [string]$builtVersionInfo.ProductVersion
if ($builtFileVersion.Trim() -ne $releaseVersion -or
    $builtProductVersion.Trim() -ne $releaseVersion) {
    throw "Spellcross.exe version mismatch: expected $releaseVersion, got FileVersion='$builtFileVersion', ProductVersion='$builtProductVersion'. Run Build-CombinedRelease.ps1 -Rebuild before packaging."
}
Write-Host "Validated executable FileVersion/ProductVersion: $releaseVersion" -ForegroundColor Green
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'dist' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$name = 'Spellcross-Reloaded-Win64-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
$stage = Join-Path $OutputDirectory $name
$stageEditor = Join-Path $stage 'Editor'
$gameData = Join-Path $stage 'data'
$editorData = Join-Path $stageEditor 'data'
New-Item -ItemType Directory -Path $gameData, $editorData -Force | Out-Null
Copy-Required $gameExe (Join-Path $stage 'Spellcross.exe')
Copy-Required $editorExe (Join-Path $stageEditor 'spellcross_map_editor.exe')

# Trace exactly which Git checkout and executable produced this package. A
# release version alone is not an adequate identifier for a bugfix build.
$gitBranch = '(no git)'
$gitCommit = '(no git)'
if (Get-Command git -ErrorAction SilentlyContinue) {
    $gitBranchCandidate = & git -C $root branch --show-current
    if ($LASTEXITCODE -eq 0 -and $gitBranchCandidate) { $gitBranch = "$gitBranchCandidate".Trim() }
    $gitCommitCandidate = & git -C $root rev-parse HEAD
    if ($LASTEXITCODE -eq 0 -and $gitCommitCandidate) { $gitCommit = "$gitCommitCandidate".Trim() }
}
$gameHash = (Get-FileHash -LiteralPath (Join-Path $stage 'Spellcross.exe') -Algorithm SHA256).Hash
@"
Git branch: $gitBranch
Git commit: $gitCommit
Game version: $releaseVersion
Built executable: $gameExe
Spellcross.exe SHA256: $gameHash
Build UTC: $((Get-Date).ToUniversalTime().ToString('o'))
"@ | Set-Content -LiteralPath (Join-Path $stage 'BUILD-INFO.txt') -Encoding UTF8
Write-Host "Package provenance: $gitBranch / $gitCommit / exe SHA256 $gameHash" -ForegroundColor Cyan

# Whitelist project-authored support files. NEVER package retail DATA archives.
$gameAssets = Join-Path $game 'data'
foreach ($f in @('C_NAMES.DEF','DEVAST.fs','PUST.fs','T11.FS',
  'font_spellcross_7pix.fnt','font_spellcross_14pix.fnt',
  'font.ttf','logo.png','units.fsa','units.json')) {
    if (Test-Path -LiteralPath (Join-Path $gameAssets $f) -PathType Leaf) {
        Copy-Item -LiteralPath (Join-Path $gameAssets $f) -Destination $gameData -Force
    }
}
Copy-Item -LiteralPath (Join-Path $gameAssets 'menu') -Destination $gameData -Recurse -Force
$movieDir = Join-Path $gameData 'movie_fallback'
New-Item -ItemType Directory -Path $movieDir -Force | Out-Null
# Local Markdown notes are intentionally not included in clean source clones.
foreach ($f in @('MANIFEST.tsv')) {
    $src = Join-Path $gameAssets ('movie_fallback\' + $f)
    if (Test-Path -LiteralPath $src -PathType Leaf) {
        Copy-Item -LiteralPath $src -Destination $movieDir -Force
    }
}
# 37 v0.8.0 H.264/AAC fallback clips are gitignored. Locate existing
# locally owned copies, optionally supplied through -MovieSource.
# Fail explicitly rather than distribute an incomplete cutscene pack.
$manifest = Join-Path $movieDir 'MANIFEST.tsv'
Assert-File $manifest
$movieCandidates = @()
if ($MovieSource) {
    $provided = (Resolve-Path -LiteralPath $MovieSource).Path
    $movieCandidates += $provided
    $movieCandidates += (Join-Path $provided 'data\movie_fallback')
    $movieCandidates += (Join-Path $provided 'movie_fallback')
}
$movieCandidates += (Join-Path $game 'builds\x64\Release\data\movie_fallback')
$movieCandidates += (Join-Path $gameAssets 'movie_fallback')
$movieCount = 0
foreach ($line in (Get-Content -LiteralPath $manifest -Encoding UTF8 | Select-Object -Skip 1)) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $fields = $line.Split([char]9)
    if ($fields.Count -lt 11) { throw "Malformed MP4 manifest entry: $line" }
    $movieFile = $fields[1].Trim()
    $expectedSha = $fields[10].Trim()
    if ([System.IO.Path]::GetFileName($movieFile) -ne $movieFile -or
        $movieFile -notmatch '^[A-Za-z0-9_-]+\.mp4$' -or
        $expectedSha -notmatch '^[a-fA-F0-9]{64}$') {
        throw "Invalid movie filename or SHA-256 in manifest: $movieFile"
    }
    $movieSrc = $null
    foreach ($candidate in $movieCandidates) {
        $p = Join-Path $candidate $movieFile
        if (Test-Path -LiteralPath $p -PathType Leaf) {
            $movieSrc = $p
            break
        }
    }
    if (-not $movieSrc) {
        throw ("Missing fallback movie " + $movieFile + ". Copy the 37 MP4 files from your v0.8.0 release to spellcross-map-edit-main\data\movie_fallback, or pass -MovieSource pointing at the old release folder.")
    }
    $actualSha = (Get-FileHash -LiteralPath $movieSrc -Algorithm SHA256).Hash
    if (-not [string]::Equals($actualSha, $expectedSha, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "MP4 integrity check failed: $movieSrc (does not match MANIFEST.tsv)"
    }
    Copy-Item -LiteralPath $movieSrc -Destination (Join-Path $movieDir $movieFile) -Force
    $movieCount++
}
if ($movieCount -ne 37) { throw "Expected 37 fallback videos, got $movieCount entries" }
Write-Host "Bundled and SHA-256 verified all $movieCount MP4 cutscenes." -ForegroundColor Green

$editorAssets = Join-Path $editor 'data'
foreach ($f in @('COMMON.fs','DEVAST.con','DEVAST.fs','PUST.con',
    'PUST.fs','T11.FS','T11.con','font_spellcross_7pix.fnt',
    'font_spellcross_14pix.fnt','units.fsa')) {
    Copy-Required (Join-Path $editorAssets $f) (Join-Path $editorData $f)
}
Copy-Item -LiteralPath (Join-Path $editorAssets 'tree_randomizer') -Destination $editorData -Recurse -Force

# Follow actual PE imports, including transitive vcpkg DLL dependencies.
if (-not $dumpbin) { throw 'dumpbin.exe not found. Required for safe runtime DLL bundling.' }
if (-not $WxRoot) { throw 'Supply -WxRoot pointing to x64 wxWidgets/vcpkg installation.' }
$wxBin = Join-Path $WxRoot 'bin'
if (-not (Test-Path -LiteralPath $wxBin -PathType Container)) {
    throw "Missing runtime DLL folder: $wxBin"
}
$copiedDlls = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
function Copy-DllImports([string]$InitialBinary, [string]$Destination) {
    $queue = New-Object 'System.Collections.Generic.Queue[string]'
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $queue.Enqueue($InitialBinary)
    while ($queue.Count -gt 0) {
        $binary = $queue.Dequeue()
        if (-not $seen.Add($binary)) { continue }
        $imports = & $dumpbin '/nologo' '/dependents' $binary 2>&1
        if ($LASTEXITCODE -ne 0) { throw "dumpbin failed: $binary" }
        foreach ($line in $imports) {
            if ([string]$line -notmatch '^\s*([a-zA-Z0-9_.-]+\.dll)\s*$') { continue }
            $dllName = $Matches[1]
            $src = Join-Path $wxBin $dllName
            if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
                if ($dllName -match '^wx(base|msw|expat|zlib|png|jpeg)') {
                    throw "wxWidgets dependency missing: $dllName in $wxBin"
                }
                continue # System DLL or system-installed MSVC redistributable
            }
            Copy-Item -LiteralPath $src -Destination (Join-Path $Destination $dllName) -Force
            [void]$copiedDlls.Add($dllName)
            $queue.Enqueue($src)
        }
    }
}
Copy-DllImports (Join-Path $stage 'Spellcross.exe') $stage
Copy-DllImports (Join-Path $stageEditor 'spellcross_map_editor.exe') $stageEditor

Copy-Required (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE-Reloaded.txt')
Copy-Required (Join-Path $editor 'LICENSE') (Join-Path $stage 'LICENSE-Map-Editor.txt')
$wxLicense = Join-Path $WxRoot 'share\wxwidgets\copyright'
if (Test-Path -LiteralPath $wxLicense -PathType Leaf) {
    $licenseDir = Join-Path $stage 'third-party-licenses'
    New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
    Copy-Item -LiteralPath $wxLicense -Destination (Join-Path $licenseDir 'wxwidgets.txt') -Force
}
# Bundle copyright notices for DLLs copied from vcpkg.
$info = Join-Path $WxRoot 'vcpkg\info'
if (Test-Path -LiteralPath $info -PathType Container) {
    $licenseDir = Join-Path $stage 'third-party-licenses'
    New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
    foreach ($manifest in (Get-ChildItem -LiteralPath $info -Filter '*_x64-windows.list' -File)) {
        $providesDll = $false
        foreach ($line in [System.IO.File]::ReadLines($manifest.FullName)) {
            if ($line -match '/bin/([^/]+\.dll)$' -and $copiedDlls.Contains($Matches[1])) {
                $providesDll = $true
                break
            }
        }
        if (-not $providesDll) { continue }
        if ($manifest.BaseName -notmatch '^(.+?)_[0-9]') { continue }
        $port = $Matches[1]
        $copyright = Join-Path $WxRoot ('share\' + $port + '\copyright')
        if (Test-Path -LiteralPath $copyright -PathType Leaf) {
            Copy-Item -LiteralPath $copyright -Destination (Join-Path $licenseDir ($port + '.txt')) -Force
        } else {
            Write-Warning "License notice missing for vcpkg package $port"
        }
    }
}
# Keep the shortcut icon and SFX source artwork under setup\ in releases.
# The desktop shortcut icon is resolved relative to the user's chosen folder.
if ($winrarExe) {
    $stageSetup = Join-Path $stage 'setup'
    New-Item -ItemType Directory -Path $stageSetup -Force | Out-Null
    foreach ($asset in @($sfxIcon, $sfxLogo, $sfxLogoHd)) {
        Copy-Required $asset (Join-Path $stageSetup (Split-Path -Leaf $asset))
    }
}
@'
SPELLCROSS RELOADED - WINDOWS X64 PORTABLE BUILD

Game:       Spellcross.exe
Map editor: Editor\spellcross_map_editor.exe

1. ZIP: unpack the whole archive into a writable folder.
   SFX EXE: choose a writable installation folder in the installer.
2. Launch Spellcross.exe first. Configure your own original Spellcross
   game archives, or download CZ/EN ISO, choose a local ISO, or use a CD.
3. The game can place imported archives in game_data\DATA.
4. Launch Editor\spellcross_map_editor.exe. It locates that game-data folder.
5. Game and editor have SEPARATE config.ini files in their executable folders.
MP4 fallback videos: 37 files, verified against MANIFEST.tsv.

Original retail game archives are NOT included. Do not confuse editor's
Editor\data\COMMON.fs with the original game's COMMON.FS archive.

Requires Microsoft Visual C++ Redistributable x64 on systems without it:
https://aka.ms/vc14/vc_redist.x64.exe

Before publishing, verify the package on a clean Windows machine.
'@ | Set-Content -LiteralPath (Join-Path $stage 'README-FIRST.txt') -Encoding UTF8
if (-not $NoZip) {
    # Stable asset name for GitHub Releases. Build folders retain timestamps
    # for troubleshooting, but the ZIP itself contains files at its root.
    $zip = Join-Path $OutputDirectory ("Spellcross_Reloaded_v$releaseVersion.zip")
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force
    Write-Host "Package created: $zip" -ForegroundColor Green
}
# Build a graphical, install-to-user-chosen-folder WinRAR SFX from the SAME
# staged contents as the ZIP. Keep the file layout flat (Spellcross.exe at root)
# to avoid broken shortcuts or creating a nested version directory.
if ($winrarExe) {
    $sfx = Join-Path $OutputDirectory ("Spellcross_Reloaded_v$releaseVersion.exe")
    $id = [Guid]::NewGuid().ToString('N')
    # Build under a normal versioned filename in an isolated work directory.
    # Publish to dist only after WinRAR has finished and the EXE was checked.
    $sfxWorkDir = Join-Path $OutputDirectory ("Spellcross-SFX-Work-$id")
    New-Item -ItemType Directory -Path $sfxWorkDir -Force | Out-Null
    $temporarySfx = Join-Path $sfxWorkDir ("Spellcross_Reloaded_v$releaseVersion.exe")
    $sfxScript = Join-Path ([System.IO.Path]::GetTempPath()) ("spellcross-sfx-$id.txt")

    # No Silent or TempMode: the user sees the folder selection dialog and
    # the game remains installed there. SavePath remembers custom destinations
    # across versions because the default Path does not contain the version.
    $sfxOptions = @"
;The comment below contains SFX script commands
Path=%USERPROFILE%\Games\Spellcross Reloaded
SavePath
Title=Spellcross Reloaded v$releaseVersion
Text
{
Spellcross Reloaded v$releaseVersion

Select the installation folder and click Install.
The game and the standalone map editor will be extracted together.
Original commercial Spellcross game data is not included.
}
Overwrite=1
Shortcut=D,Spellcross.exe,,Spellcross Reloaded,Spellcross Reloaded,setup\spellcross_reloaded.ico
"@
    try {
        # ASCII is intentional: SFX config commands and displayed text here
        # are ASCII-only, so no BOM/codepage ambiguity with WinRAR.
        [System.IO.File]::WriteAllText($sfxScript, $sfxOptions,
            [System.Text.Encoding]::ASCII)
        Write-Host "Creating SFX installer: $sfx"
        # IMPORTANT: WinRAR.exe is a GUI application. Calling it with '&' may
        # return to PowerShell before the SFX has finished being written.
        # Start-Process -Wait waits for WinRAR and its child processes; only
        # after that may we validate, move or delete the temporary archive.
        # Quote switch values and the output path explicitly: paths can contain
        # spaces (e.g. C:\Program Files or the user's repository directory).
        $rarArguments = @(
            'a', '-cfg-', '-r', '-y', '-m5', '-sfxDefault.SFX',
            ('-iimg"{0}"' -f $sfxLogo),
            ('-iimg"{0}"' -f $sfxLogoHd),
            ('-iicon"{0}"' -f $sfxIcon),
            ('-z"{0}"' -f $sfxScript),
            ('"{0}"' -f $temporarySfx), '*'
        ) -join ' '
        Write-Host 'Waiting for WinRAR to finish creating the SFX archive...'
        Write-Verbose ("WinRAR: {0} {1}" -f $winrarExe, $rarArguments)
        $rarProcess = Start-Process -FilePath $winrarExe -WorkingDirectory $stage `
            -ArgumentList $rarArguments -PassThru -Wait
        if ($rarProcess.ExitCode -ne 0) {
            throw "WinRAR SFX creation failed (exit code $($rarProcess.ExitCode))."
        }
        if (-not (Test-Path -LiteralPath $temporarySfx -PathType Leaf)) {
            $producedFiles = @(Get-ChildItem -LiteralPath $sfxWorkDir -File |
                Select-Object -ExpandProperty Name) -join ', '
            throw "WinRAR exited with code 0 but no SFX EXE was found at $temporarySfx. Files found in work directory: $producedFiles"
        }
        # Validate that the output is an actual Windows executable before publishing.
        $stream = [System.IO.File]::OpenRead($temporarySfx)
        try {
            $signature = New-Object byte[] 2
            if ($stream.Read($signature, 0, 2) -ne 2 -or
                $signature[0] -ne 0x4D -or $signature[1] -ne 0x5A) {
                throw "WinRAR output is not an EXE: $temporarySfx"
            }
        } finally {
            $stream.Dispose()
        }
        Move-Item -LiteralPath $temporarySfx -Destination $sfx -Force
        Write-Host "SFX installer created: $sfx" -ForegroundColor Green
    } finally {
        Remove-Item -LiteralPath $sfxScript -Force -ErrorAction SilentlyContinue
        # Safe now: Start-Process -Wait finished before this cleanup.
        Remove-Item -LiteralPath $sfxWorkDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
Write-Host "Staged release: $stage; copied $($copiedDlls.Count) distinct DLLs." -ForegroundColor Green
if ($PruneOldBuilds) {
    # The release is complete. Keep 3 most recent dated staging builds.
    # Stable, versioned ZIP and EXE releases are never removed by cleanup.
    & (Join-Path $PSScriptRoot 'Clean-OldBuilds.ps1') -DistDirectory $OutputDirectory -Keep 3 -Apply
}
