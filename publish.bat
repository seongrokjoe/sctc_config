@echo off
echo ============================================
echo  SCTC_CONFIG - Release Build
echo ============================================
echo.

cd /d "%~dp0"

dotnet publish SCTC_CONFIG\SCTC_CONFIG.csproj -c Release -r win-x64 --no-self-contained -p:PublishSingleFile=true -o dist

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo [OK] Build succeeded.
echo Output: %~dp0dist\SCTC_CONFIG.exe
echo.
pause
