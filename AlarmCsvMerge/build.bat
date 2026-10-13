@echo off
setlocal
title Build AlarmCsvMerge
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Test %*
set "BUILD_EXIT=%ERRORLEVEL%"
echo.
if not "%BUILD_EXIT%"=="0" (
    echo BUILD FAILED. Read the error above. No new release was published.
) else (
    echo Build finished. See the output path above.
)
echo.
pause
exit /b %BUILD_EXIT%
