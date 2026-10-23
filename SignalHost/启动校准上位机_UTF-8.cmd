@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

where python.exe >nul 2>nul
if errorlevel 1 goto python_missing

if /I "%SIGNALHOST_STARTUP_CHECK%"=="1" (
    python.exe -c "from calibration_host.gui import create_application; root, workbench = create_application(); root.withdraw(); root.update_idletasks(); root.destroy(); print('DOUBLE_CLICK_LAUNCHER_PASS')"
    if errorlevel 1 exit /b 1
    exit /b 0
)

python.exe -m calibration_host.app
if errorlevel 1 goto launch_failed
exit /b 0

:python_missing
echo Python was not found in PATH.
echo Expected project: F:\STM32\SignalHost
pause
exit /b 1

:launch_failed
echo SignalHost calibration GUI failed to start.
echo Run tools\run_calibration_tests.ps1 for diagnostics.
pause
exit /b 1
