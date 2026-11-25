@echo off
echo ============================================
echo  SCTC_CONFIG - Release Build
echo ============================================
echo.

cd /d "%~dp0"

rem Clear old output so files from earlier publishes do not remain.
if exist dist rmdir /s /q dist

rem Settings (self-contained, single file, compression) live in the publish profile.
dotnet publish SCTC_CONFIG\SCTC_CONFIG.csproj -c Release -p:PublishProfile=SingleFileRelease

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

for %%F in ("%~dp0dist\SCTC_CONFIG.exe") do set /a EXE_MB=%%~zF / 1048576

echo.
echo [OK] Build succeeded.
echo Output: %~dp0dist\SCTC_CONFIG.exe (%EXE_MB% MB)
echo Copy this single exe to the target PC. No .NET installation is needed.
echo.
pause
