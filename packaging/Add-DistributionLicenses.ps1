# The contents of this file are subject to the Common Public Attribution License Version 1.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at https://opensource.org/license/cpal-1-0. The License is based on the Mozilla Public License Version 1.1 but Sections 14 and 15 have been added to cover use of software over a computer network and provide for limited attribution for the Original Developer. In addition, Exhibit A has been modified to be consistent with Exhibit B.
# Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the specific language governing rights and limitations under the License.
# The Original Code is Spellcross Reloaded restoration code covered by CPAL-1.0.
# The Initial Developer and Original Developer of the Original Code is Luboš Horák. All portions of the code written by Luboš Horák and covered by this License are Copyright (c) 2026 Luboš Horák. All Rights Reserved.
# Contributor: see AUTHORS.md and the Git history.
# SPDX-License-Identifier: CPAL-1.0
# Previously granted MIT rights are not withdrawn; see LICENSING.md.

# Copy attribution, dependency notices and corresponding build source.
# The ZIP contains the actual working source, including local changes in the
# universal builder. Never copy the user's retail archives, saves or config.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Root,
    [Parameter(Mandatory=$true)][string]$EditorRoot,
    [Parameter(Mandatory=$true)][string]$Stage,
    [Parameter(Mandatory=$true)][string]$WxRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Copy-Notice([string]$source, [string]$target) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required notice missing: $source" }
    Copy-Item -LiteralPath $source -Destination $target -Force
}
$licenses = Join-Path $Stage 'third-party-licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
foreach ($name in @('AUTHORS.md','NOTICE.md','LICENSING.md','THIRD_PARTY_NOTICES.md','CHANGES.md')) {
    Copy-Notice (Join-Path $Root $name) (Join-Path $Stage $name)
}
Copy-Item -LiteralPath (Join-Path $Root 'LICENSES') -Destination $Stage -Recurse -Force
Copy-Notice (Join-Path $Root 'third_party\libsmacker\COPYING') (Join-Path $licenses 'libsmacker-LGPL-2.1.txt')
Copy-Notice (Join-Path $Root 'LICENSES\LGPL-2.0.txt') (Join-Path $licenses 'XMIDI-LGPL-2.0.txt')
# Extract the full copyright + permission + disclaimer from vendored headers.
$headers = @{
    'RtAudio.txt' = 'RtAudio.h'
    'SimpleIni.txt' = 'SimpleIni.h'
    'cxxmidi.txt' = 'cxxmidi\file.hpp'
}
foreach ($name in $headers.Keys) {
    $header = Get-Content -Raw -LiteralPath (Join-Path (Join-Path $Root 'spellcross-remaster-main\source') $headers[$name])
    $license = [regex]::Match($header, '(?s)Copyright \(c\).*?THE SOFTWARE IS PROVIDED.*?SOFTWARE\.')
    if (-not $license.Success) { throw "Cannot extract required license: $name" }
    Set-Content -LiteralPath (Join-Path $licenses $name) -Value $license.Value -Encoding UTF8
}
# Build source (including source for statically compiled LGPL components)
# is delivered with each binary distribution; recipients can rebuild/relink.
# The explicitly staged support data is reused, not the user's original DATA.
$temp = Join-Path ([IO.Path]::GetTempPath()) ('spellcross-source-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
function Copy-CodeTree([string]$from, [string]$to) {
    if (-not (Test-Path -LiteralPath $from -PathType Container)) { throw "Missing build source: $from" }
    $from = (Resolve-Path -LiteralPath $from).Path.TrimEnd('\','/')
    foreach ($file in Get-ChildItem -LiteralPath $from -Recurse -File) {
        $relative = $file.FullName.Substring($from.Length + 1)
        # Only source, build definitions, embedded resources and build scripts.
        # Exclude generated trees, local IDE state and raw game-data catalog.
        if ($relative -match '(^|[\\/])(\.git|\.vs|builds|x64|Debug|Release|spellcross_ui_coordinate_catalog)([\\/]|$)') { continue }
        if ($file.Extension -notmatch '^\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl|inc|rc|ico|bmp|png|sln|vcxproj|filters|props|targets|natvis|ps1|py|md|txt)$' -and
            $file.Name -notin @('COPYING','LICENSE','config.example.ini')) { continue }
        if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Unexpected linked source: $($file.FullName)" }
        $destination = Join-Path $to $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
    }
}
try {
    $gameSource = Join-Path $temp 'spellcross-remaster-main'
    $editorSource = Join-Path $temp 'external\map-editor'
    New-Item -ItemType Directory -Path $gameSource,$editorSource -Force | Out-Null
    Copy-CodeTree (Join-Path $Root 'spellcross-remaster-main\source') (Join-Path $gameSource 'source')
    Copy-CodeTree (Join-Path $EditorRoot 'source') (Join-Path $editorSource 'source')
    Copy-CodeTree (Join-Path $Root 'third_party') (Join-Path $temp 'third_party')
    Copy-CodeTree (Join-Path $Root 'packaging') (Join-Path $temp 'packaging')
    foreach ($name in @('LICENSE','README.md','AUTHORS.md','NOTICE.md','LICENSING.md','THIRD_PARTY_NOTICES.md','CHANGES.md')) {
        Copy-Notice (Join-Path $Root $name) (Join-Path $temp $name)
    }
    Copy-Notice (Join-Path $Root 'spellcross-remaster-main\LICENSE') (Join-Path $gameSource 'LICENSE')
    Copy-Notice (Join-Path $EditorRoot 'LICENSE') (Join-Path $editorSource 'LICENSE')
    Copy-Item -LiteralPath (Join-Path $Root 'LICENSES') -Destination $temp -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $Stage 'data') -Destination $gameSource -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $Stage 'locales') -Destination $gameSource -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $Stage 'Editor\data') -Destination $editorSource -Recurse -Force
    # Source-only build instructions work without a Git checkout or a remote.
    Copy-Notice (Join-Path $Root 'packaging\SOURCE-AND-RELINKING.txt') (Join-Path $temp 'SOURCE-AND-RELINKING.txt')
    Copy-Notice (Join-Path $Root 'packaging\SOURCE-AND-RELINKING.txt') (Join-Path $Stage 'SOURCE-AND-RELINKING.txt')
    $archive = Join-Path $Stage 'SOURCE-CODE.zip'
    Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $archive -CompressionLevel Optimal -Force
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw 'Corresponding source archive was not created.' }
    $sourceHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    Add-Content -LiteralPath (Join-Path $Stage 'BUILD-INFO.txt') -Value "Corresponding source ZIP SHA256: $sourceHash" -Encoding UTF8
    Write-Host "Included corresponding source archive and attribution notices." -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
