[CmdletBinding()]
param([string]$Report)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Report) { $Report = Join-Path $root 'logs/floating-point-fault.txt' }
$symbolizer = Join-Path $root '.tools/llvm/bin/llvm-symbolizer.exe'
foreach ($line in Get-Content -LiteralPath $Report) {
    if ($line -match 'rva=(0x[0-9a-fA-F]+) module=(.+n3\.exe)$') {
        & $symbolizer --relative-address "--obj=$($Matches[2])" $Matches[1]
    }
}
