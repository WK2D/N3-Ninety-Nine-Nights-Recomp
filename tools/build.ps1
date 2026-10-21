[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'RelWithDebInfo',
    [ValidateRange(1,64)][int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$localTools = Join-Path $root '.tools'
$cmake = "$localTools/cmake/cmake-3.31.8-windows-x86_64/bin/cmake.exe"
if (!(Test-Path $cmake)) { throw 'Run tools/bootstrap.ps1 first.' }
$saved = @{}
foreach ($name in @('PATH','INCLUDE','LIB','VCToolsInstallDir','WindowsSdkDir','WindowsSDKVersion')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
Push-Location $root
try {
    if (Test-Path "$localTools/msvc/setup_x64.bat") {
        $vc = Get-ChildItem "$localTools/msvc/VC/Tools/MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
        $kit = "$localTools/msvc/Windows Kits/10"
        $version = (Get-ChildItem "$kit/Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
        $env:VCToolsInstallDir = "$($vc.FullName)/"
        $env:WindowsSdkDir = "$kit/"
        $env:WindowsSDKVersion = "$version/"
        $env:INCLUDE = "$($vc.FullName)/include;$kit/Include/$version/ucrt;$kit/Include/$version/shared;$kit/Include/$version/um;$kit/Include/$version/winrt"
        $env:LIB = "$($vc.FullName)/lib/x64;$kit/Lib/$version/ucrt/x64;$kit/Lib/$version/um/x64"
        $env:PATH = "$($vc.FullName)/bin/Hostx64/x64;$kit/bin/$version/x64;$env:PATH"
    }
    $env:PATH = "$localTools/llvm/bin;$localTools/ninja;$env:PATH"
    & (Join-Path $PSScriptRoot 'codegen.ps1')
    $preset = 'win-amd64-' + $Configuration.ToLowerInvariant()
    & $cmake --preset $preset "-DCMAKE_PREFIX_PATH=$localTools/rexglue-sdk/win-amd64"
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    & $cmake --build --preset $preset --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Native build failed.' }
    & "$localTools/cmake/cmake-3.31.8-windows-x86_64/bin/ctest.exe" --test-dir "out/build/$preset" --output-on-failure
    if ($LASTEXITCODE) { throw 'Native regression test failed.' }
    Write-Host "Built out/build/$preset/n3.exe"
} finally {
    Pop-Location
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
