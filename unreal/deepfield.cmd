@echo off
rem Deep Field 3D (Unreal) on Windows: double-click to set this machine up and build,
rem or run from a terminal:  deepfield [setup|doctor|build|test|pr-check|check|editor|play|host|join|solution|help]
rem Everything is in Build\deepfield.ps1; this file only starts it past the execution policy.
setlocal
set "DF_PAUSE="
rem Double-clicked (no arguments, started by Explorer): keep the window open at the end.
if "%~1"=="" (echo %cmdcmdline% | findstr /i /c:"%~nx0" >nul && set "DF_PAUSE=-Pause")
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build\deepfield.ps1" %* %DF_PAUSE%
exit /b %errorlevel%
