[CmdletBinding()]
param([switch]$Regenerate)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$sdk = Join-Path $root '.tools/rexglue-sdk/win-amd64/bin/rexglue.exe'
if (!(Test-Path $sdk)) { throw 'Run tools/bootstrap.ps1 first.' }
$expected = Get-Content (Join-Path $root 'docs/game-build.json') -Raw | ConvertFrom-Json
$xex = Join-Path $root 'private/game/default.xex'
if (!(Test-Path $xex)) { throw 'Extract the game first; see README.md.' }
if ((Get-FileHash $xex -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected.xexSha256) {
    throw 'Unsupported executable revision. Review the function hints before adding another build.'
}
Push-Location $root
try {
    New-Item -ItemType Directory -Force logs | Out-Null
    $arguments = @('--log-file', 'logs/codegen.log', 'codegen', 'n3_manifest.toml')
    if ($Regenerate) { $arguments += '--ignore-stamp' }
    & $sdk @arguments
    if ($LASTEXITCODE) { throw "Strict code generation failed ($LASTEXITCODE). See logs/codegen.log." }
} finally { Pop-Location }
