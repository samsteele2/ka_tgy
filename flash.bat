@echo off
setlocal

rem Flash the reviewed ka_nfet.hex using the implementation under scripts.
rem The PowerShell script resolves the repository root from its own location.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\flash.ps1"
set "flashExit=%ERRORLEVEL%"

echo.
if not "%flashExit%"=="0" (
    echo Flash failed with exit code %flashExit%.
) else (
    echo Flash and verification completed successfully.
)
pause
exit /b %flashExit%
