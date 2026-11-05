@echo off
rem Builds every plugin and installs the VST3s for Cubase and other hosts: scripts\install.ps1.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\install.ps1"
if errorlevel 1 ( echo. & echo NOT EVERYTHING INSTALLED: see the lines above & pause & exit /b 1 )
pause
