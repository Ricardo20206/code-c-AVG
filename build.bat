@echo off
REM Script de build Ventec AGV Monitor
REM Installe PlatformIO si absent, compile et upload

where pio >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Installation de PlatformIO...
    pip install platformio
)

cd /d "%~dp0"
echo Compilation...
pio run
if %ERRORLEVEL% NEQ 0 exit /b 1

echo.
echo Compilation OK. Pour flasher : pio run -t upload
echo Pour le moniteur serie : pio device monitor
pause
