[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$parts = @('demo.dat.001','demo.dat.002')
$source = Join-Path $PSScriptRoot '..\data'
$target = [System.IO.Path]::GetFullPath($Destination)
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($target)) | Out-Null
$temp = $target + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
try {
    $output = [System.IO.File]::Create($temp)
    try {
        foreach ($part in $parts) {
            $partStream = [System.IO.File]::OpenRead((Join-Path $source $part))
            try { $partStream.CopyTo($output) } finally { $partStream.Dispose() }
        }
    } finally { $output.Dispose() }
    $expected = 'C5CAC62017AF8A775896A3149D98D4BF50D5E454F031333DA9AA3BD0569E28FC'
    if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ne $expected) {
        throw 'Bundled demo parts are incomplete or damaged.'
    }
    Move-Item -LiteralPath $temp -Destination $target -Force
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
}
