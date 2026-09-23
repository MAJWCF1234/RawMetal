@echo off
setlocal EnableExtensions
title DEPTHWORKS MAP INJECTOR

if "%~1"=="" (
    echo =====================================================================
    echo    DEPTHWORKS // LOW-LEVEL ENGINE MAP INJECTOR
    echo =====================================================================
    echo.
    echo [!] Drag and drop a Map Payload .txt file onto this script.
    echo     Or run: InstallMap.cmd ^<path-to-map.txt^>
    echo.
    pause
    exit /b 1
)

set "PAYLOAD_FILE=%~f1"
pushd "%~dp0" >nul

echo =====================================================================
echo    DEPTHWORKS // LOW-LEVEL ENGINE MAP INJECTOR
echo =====================================================================
echo.

if not exist "%PAYLOAD_FILE%" (
    echo [!] Error: Payload file "%PAYLOAD_FILE%" not found.
    popd
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "tools\InstallMap.ps1" -Payload "%PAYLOAD_FILE%" -Mode Info
if errorlevel 1 (
    echo.
    echo [!] Payload validation failed.
    popd
    pause
    exit /b 1
)

echo.
echo ---------------------------------------------------------------------
echo  SELECT INSTALLATION DESTINATION:
echo ---------------------------------------------------------------------
echo   [1] Install using META_DEFAULT_TARGET ^(recommended^)
echo   [2] Install into Main Campaign ^(World.cpp + rebuild^)
echo   [3] Install as Playable Custom Campaign ^(no rebuild^)
echo   [4] Abort Installation
echo ---------------------------------------------------------------------
choice /c 1234 /n /m " Select target destination [1, 2, 3, or 4]: "

if errorlevel 4 goto :ABORT
if errorlevel 3 goto :INSTALL_CUSTOM
if errorlevel 2 goto :INSTALL_MAIN
goto :INSTALL_AUTO

:INSTALL_AUTO
echo.
echo [*] Routing according to META_DEFAULT_TARGET...
rem Use the installer's metadata parser for encoding, whitespace and line endings.
rem Never fall back to Main when metadata cannot be resolved.
set "RESOLVED_TARGET="
for /f "delims=" %%T in ('powershell -NoProfile -ExecutionPolicy Bypass -File "tools\InstallMap.ps1" -Payload "%PAYLOAD_FILE%" -Mode Resolve') do set "RESOLVED_TARGET=%%T"
if /i "%RESOLVED_TARGET%"=="CUSTOM" goto :INSTALL_CUSTOM
if /i "%RESOLVED_TARGET%"=="MAIN" goto :INSTALL_MAIN
echo [!] Could not resolve the installation destination. Nothing was installed.
popd
pause
exit /b 1

:INSTALL_MAIN
echo.
echo [*] Installing payload into src\world\World.cpp...
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\InstallMap.ps1" -Payload "%PAYLOAD_FILE%" -Mode Main
if errorlevel 1 (
    echo [!] Main-campaign injection failed. World.cpp was not changed, or was restored from backup.
    popd
    pause
    exit /b 1
)

echo.
echo [*] Triggering release build...
call Build.cmd
if errorlevel 1 (
    echo.
    echo [!] Build failed. The injected source is still present for repair.
    echo     Backup: src\world\World.cpp.bak
    popd
    pause
    exit /b 1
)

echo.
echo [OK] Main campaign map installed and build completed.
goto :END

:INSTALL_CUSTOM
echo.
echo [*] Installing playable custom campaign...
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\InstallMap.ps1" -Payload "%PAYLOAD_FILE%" -Mode Custom
if errorlevel 1 (
    echo [!] Custom campaign installation failed.
    popd
    pause
    exit /b 1
)
goto :END

:ABORT
echo.
echo [!] Operation aborted by user.
popd
exit /b 0

:END
echo.
echo =====================================================================
echo    PROCESS COMPLETED
echo =====================================================================
popd
pause
exit /b 0
