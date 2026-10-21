[CmdletBinding()]
param(
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe',
    [string]$Python = 'python',
    [switch]$AcceptVisualStudioLicense,
    [switch]$ToolsOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$localTools = Join-Path $root '.tools'
$lock = Get-Content (Join-Path $PSScriptRoot 'toolchain.lock.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force $localTools | Out-Null

function Get-VerifiedDownload($Package, [string]$Name) {
    $destination = Join-Path $localTools $Name
    if (!(Test-Path -LiteralPath $destination) -or
        (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Package.sha256) {
        Write-Host "Downloading $Name"
        Invoke-WebRequest $Package.url -OutFile $destination
    }
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Package.sha256) {
        throw "Checksum mismatch: $Name"
    }
    return $destination
}

if (!(Test-Path "$localTools/rexglue-sdk/win-amd64/bin/rexglue.exe")) {
    $archive = Get-VerifiedDownload $lock.sdk 'rexglue-sdk.zip'
    Expand-Archive -LiteralPath $archive -DestinationPath "$localTools/rexglue-sdk" -Force
}
if (!(Test-Path "$localTools/llvm/bin/clang++.exe")) {
    if (!(Test-Path -LiteralPath $SevenZip)) { throw '7-Zip is required to unpack portable LLVM. Pass -SevenZip <path>.' }
    $archive = Get-VerifiedDownload $lock.llvm 'LLVM-20.1.8-win64.exe'
    & $SevenZip x $archive "-o$localTools/llvm" -y -bso0 -bsp0
    if ($LASTEXITCODE) { throw 'LLVM extraction failed' }
}
if (!(Test-Path "$localTools/cmake/cmake-3.31.8-windows-x86_64/bin/cmake.exe")) {
    $archive = Get-VerifiedDownload $lock.cmake 'cmake.zip'
    Expand-Archive -LiteralPath $archive -DestinationPath "$localTools/cmake" -Force
}
if (!(Test-Path "$localTools/ninja/ninja.exe")) {
    $archive = Get-VerifiedDownload $lock.ninja 'ninja.zip'
    Expand-Archive -LiteralPath $archive -DestinationPath "$localTools/ninja" -Force
}
if (!$ToolsOnly -and !(Test-Path "$localTools/msvc/setup_x64.bat")) {
    if (!$AcceptVisualStudioLicense) {
        throw 'Portable Microsoft tools require their licence. Run with -AcceptVisualStudioLicense or use -ToolsOnly with your own VS 2022 environment.'
    }
    $downloader = Get-VerifiedDownload $lock.msvc_downloader 'portable-msvc.py'
    & $Python (Join-Path $PSScriptRoot 'portable-msvc-guard.py') $downloader
    if ($LASTEXITCODE) { throw 'Portable MSVC setup failed' }
}
Write-Host 'Project-local toolchain ready.'
