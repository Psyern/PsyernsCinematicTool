@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build_PCT.ps1" %*
set "PCT_EXITCODE=%ERRORLEVEL%"
pause
exit /b %PCT_EXITCODE%
