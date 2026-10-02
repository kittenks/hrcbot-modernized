@echo off
REM ===========================================================================
REM HRCBot modernized - Windows build wrapper.  Delegates to build.ps1.
REM Usage: build.bat [all^|x86^|x64] [-Setup]
REM ===========================================================================
setlocal
set ARG=%1
if "%ARG%"=="" set ARG=all
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Arch %ARG%
endlocal
