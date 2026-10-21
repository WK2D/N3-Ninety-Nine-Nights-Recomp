[CmdletBinding()]
param(
    [string]$Report,
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'RelWithDebInfo',
    [string]$Executable
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Report) {
    $candidates = @(
        (Join-Path $root 'private/user/logs/floating-point-fault.txt'),
        (Join-Path $root 'logs/floating-point-fault.txt'),
        (Join-Path $env:LOCALAPPDATA 'N3Recompiled/logs/floating-point-fault.txt')
    )
    $Report = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (!$Report) { throw 'No floating-point fault report found. Pass -Report to select one.' }
}
if (!$Executable) {
    $preset = 'win-amd64-' + $Configuration.ToLowerInvariant()
    $Executable = Join-Path $root "out/build/$preset/n3.exe"
}
$symbolizer = Join-Path $root '.tools/llvm/bin/llvm-symbolizer.exe'
foreach ($line in Get-Content -LiteralPath $Report) {
    if ($line -match 'rva=(0x[0-9a-fA-F]+) module=(n3\.exe|.*[\\/]n3\.exe)$') {
        & $symbolizer --relative-address "--obj=$Executable" $Matches[1]
    }
}
