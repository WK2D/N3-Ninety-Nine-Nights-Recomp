[CmdletBinding()]
param(
    [ValidateRange(1,60)][int]$Seconds = 20,
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'RelWithDebInfo'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$preset = 'win-amd64-' + $Configuration.ToLowerInvariant()
$executable = Join-Path $root "out/build/$preset/n3.exe"
if (!(Test-Path $executable)) { throw 'Build n3.exe first.' }
New-Item -ItemType Directory -Force (Join-Path $root 'logs'), (Join-Path $root 'private/user') | Out-Null
$log = Join-Path $root ('logs/runtime-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
$info = [Diagnostics.ProcessStartInfo]::new()
$info.FileName = $executable
$info.WorkingDirectory = $root
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
foreach ($argument in @('--headless', '--no-fullscreen', '--window_width', '1280', '--window_height', '720',
    '--game_data_root', (Join-Path $root 'private/game'),
    '--user_data_root', (Join-Path $root 'private/user'), '--cache_root', (Join-Path $root 'private/cache'),
    '--log_file', $log, '--log_level', 'debug', '--log_flush_interval', '1')) {
    $info.ArgumentList.Add($argument)
}
$process = [Diagnostics.Process]::Start($info)
$timedOut = !$process.WaitForExit($Seconds * 1000)
if ($timedOut) { $process.Kill(); $process.WaitForExit() }
$result = [ordered]@{
    executable = $executable
    observedSeconds = $Seconds
    stoppedByHarness = $timedOut
    exitCode = $process.ExitCode
    log = $log
    note = 'Process survival alone does not establish successful boot or gameplay.'
}
$process.Dispose()
$result | ConvertTo-Json | Set-Content (Join-Path $root 'logs/smoke-result.json')
$result | ConvertTo-Json
if (Test-Path $log) { Get-Content $log -Tail 45 }
