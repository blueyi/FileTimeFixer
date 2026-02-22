@echo off
REM Run env init with script execution allowed (no need to change system policy).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0init_env.ps1"
if errorlevel 1 exit /b 1
