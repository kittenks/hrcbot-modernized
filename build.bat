@echo off
REM ===========================================================================
REM HRCBot modernized - Windows build wrapper.  Delegates to build.ps1.
REM Usage: build.bat [all|x86|x64] [-Setup]
REM ===========================================================================
setlocal
set ARCH=all
set SETUP=
:parse
if "%~1"=="" goto done
if /i "%~1"=="--setup"  set SETUP=-Setup
if /i "%~1"=="-setup"   set SETUP=-Setup
if /i "%~1"=="-Setup"   set SETUP=-Setup
if /i "%~1"=="x86"      set ARCH=x86
if /i "%~1"=="x64"      set ARCH=x64
if /i "%~1"=="all"      set ARCH=all
shift
goto parse
:done
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Arch %ARCH% %SETUP%
set EXITCODE=%ERRORLEVEL%
endlocal
exit /b %EXITCODE%
