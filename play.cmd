@echo off
rem Double-click / cmd.exe entry point: hands everything to play.ps1 without
rem needing the execution policy relaxed for the whole machine.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play.ps1" %*
exit /b %ERRORLEVEL%
