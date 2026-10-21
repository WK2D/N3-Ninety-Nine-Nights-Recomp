[CmdletBinding()]
param([string]$Name = 'N3 Recompiled')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or !$Name.Trim()) {
    throw 'Shortcut name must be a valid file name.'
}
$desktop = [Environment]::GetFolderPath('DesktopDirectory')
if (!$desktop -or !(Test-Path -LiteralPath $desktop -PathType Container)) { throw 'Desktop folder not found.' }
$hostExecutable = (Get-Command pwsh -ErrorAction Stop).Source
$shortcutPath = Join-Path $desktop "$Name.lnk"
$shell = New-Object -ComObject WScript.Shell
try {
    $shortcut = $shell.CreateShortcut($shortcutPath)
    try {
        $shortcut.TargetPath = $hostExecutable
        $shortcut.Arguments = '-NoLogo -NoProfile -WindowStyle Hidden -File "' + (Join-Path $PSScriptRoot 'launch.ps1') + '"'
        $shortcut.WorkingDirectory = $root
        $shortcut.Description = 'Launch the current N3 Recompiled build in a 1280 x 720 window'
        $shortcut.IconLocation = (Join-Path $root 'out/build/win-amd64-relwithdebinfo/n3.exe') + ',0'
        $shortcut.Save()
    } finally { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($shortcut) }
} finally { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell) }
Write-Output $shortcutPath
