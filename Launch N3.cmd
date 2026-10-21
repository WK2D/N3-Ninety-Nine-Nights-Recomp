@echo off
pwsh -NoProfile -File "%~dp0tools\launch.ps1" %*
if errorlevel 1 pause
