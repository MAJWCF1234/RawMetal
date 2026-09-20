@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal Master
cd /d "%~dp0"
set "ROOT=%CD%"

rem ============================================================
rem RawMetal Master
rem Central launcher for day-to-day RawMetal development.
rem
rem Supports:
rem   Master.cmd
rem   Master.cmd play
rem   Master.cmd build
rem   Master.cmd test
rem   Master.cmd sync
rem   Master.cmd ship
rem   Master.cmd clean
rem   Master.cmd status
rem ============================================================

if not "%~1"=="" goto :dispatch

:menu
cls
echo ============================================================
echo   RAWMETAL MASTER
echo ============================================================
echo.
echo Root: %ROOT%
echo.
echo   [1] PLAY
echo       Launch RawMetal normally.
echo.
echo   [2] BUILD
echo       Build Release RawMetal.exe.
echo.
echo   [3] AUTOMATED TESTING
echo       Build + Stalker + Physics/AI + Save + Vulkan smoke tests.
echo       All generated images/logs go into diagnostics\TIMESTAMP.
echo.
echo   [4] GITHUB SYNC
echo       Open the smart FROM / TO / BOTH sync menu.
echo.
echo   [5] SHIP RELEASE
echo       Build, verify size, push source, and publish release.
echo.
echo   [6] CLEAN ROOT JUNK
echo       Move generated RawMetal logs and loose PPM files to diagnostics.
echo       Does NOT touch source, EXE, ZIP, Git, or build files.
echo.
echo   [7] STATUS
echo       Git status + EXE/ZIP sizes + latest diagnostics run.
echo.
echo   [8] OPEN PROJECT FOLDER
echo.
echo   [9] OPEN DIAGNOSTICS
echo.
echo   [Q] QUIT
echo.
choice /C 123456789Q /N /M "Choose: "

if errorlevel 10 exit /b 0
if errorlevel 9 goto :open_diagnostics
if errorlevel 8 goto :open_root
if errorlevel 7 goto :status
if errorlevel 6 goto :clean
if errorlevel 5 goto :ship
if errorlevel 4 goto :sync
if errorlevel 3 goto :test
if errorlevel 2 goto :build
if errorlevel 1 goto :play
goto :menu

:dispatch
if /I "%~1"=="play" goto :play_cli
if /I "%~1"=="run" goto :play_cli
if /I "%~1"=="build" goto :build_cli
if /I "%~1"=="test" goto :test_cli
if /I "%~1"=="tests" goto :test_cli
if /I "%~1"=="sync" goto :sync_cli
if /I "%~1"=="ship" goto :ship_cli
if /I "%~1"=="release" goto :ship_cli
if /I "%~1"=="clean" goto :clean_cli
if /I "%~1"=="status" goto :status_cli
echo [ERROR] Unknown command: %~1
echo.
echo Valid commands:
echo   play  build  test  sync  ship  clean  status
exit /b 2

:play
call :play_core
call :pause_menu
goto :menu

:build
call :build_core
call :pause_menu
goto :menu

:test
call :test_core
call :pause_menu
goto :menu

:sync
call :sync_core
call :pause_menu
goto :menu

:ship
call :ship_core
call :pause_menu
goto :menu

:clean
call :clean_core
call :pause_menu
goto :menu

:status
call :status_core
call :pause_menu
goto :menu

:open_root
start "" "%ROOT%"
goto :menu

:open_diagnostics
if not exist "%ROOT%\diagnostics" mkdir "%ROOT%\diagnostics" >nul 2>&1
start "" "%ROOT%\diagnostics"
goto :menu

:play_cli
call :play_core
exit /b %ERRORLEVEL%

:build_cli
call :build_core
exit /b %ERRORLEVEL%

:test_cli
call :test_core
exit /b %ERRORLEVEL%

:sync_cli
call :sync_core
exit /b %ERRORLEVEL%

:ship_cli
call :ship_core
exit /b %ERRORLEVEL%

:clean_cli
call :clean_core
exit /b %ERRORLEVEL%

:status_cli
call :status_core
exit /b %ERRORLEVEL%

:play_core
echo.
echo ============================================================
echo   PLAY RAWMETAL
echo ============================================================
echo.

if exist "%ROOT%\RawMetal.cmd" (
    call "%ROOT%\RawMetal.cmd"
    exit /b 0
)

if exist "%ROOT%\RawMetal.exe" (
    start "" "%ROOT%\RawMetal.exe"
    exit /b 0
)

echo [ERROR] RawMetal.exe was not found.
echo Run BUILD first.
exit /b 1

:build_core
echo.
echo ============================================================
echo   BUILD RAWMETAL
echo ============================================================
echo.

if not exist "%ROOT%\Build.cmd" (
    echo [ERROR] Build.cmd is missing.
    exit /b 1
)

call "%ROOT%\Build.cmd"
if errorlevel 1 (
    echo.
    echo [FAIL] Build failed.
    exit /b 1
)

if not exist "%ROOT%\RawMetal.exe" (
    echo [FAIL] Build completed but RawMetal.exe is missing.
    exit /b 1
)

for %%A in ("%ROOT%\RawMetal.exe") do set "EXE_BYTES=%%~zA"
echo.
echo [OK] Build complete.
echo RawMetal.exe: !EXE_BYTES! bytes
exit /b 0

:test_core
echo.
echo ============================================================
echo   AUTOMATED TESTING
echo ============================================================
echo.

rem If the dedicated tester exists, use it.
if exist "%ROOT%\AutomatedTest.cmd" (
    echo [INFO] Using AutomatedTest.cmd
    call "%ROOT%\AutomatedTest.cmd"
    exit /b %ERRORLEVEL%
)

rem Otherwise run the same important suite directly from this master.
echo [INFO] AutomatedTest.cmd is not installed.
echo        Using the Master's built-in test runner.
echo.

call :build_core
if errorlevel 1 exit /b 1

for /f %%A in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH-mm-ss"') do set "STAMP=%%A"
set "RUN_DIR=%ROOT%\diagnostics\%STAMP%"
mkdir "%RUN_DIR%" >nul 2>&1

if not exist "%RUN_DIR%" (
    echo [FAIL] Could not create:
    echo %RUN_DIR%
    exit /b 1
)

set "RESULTS=%RUN_DIR%\TEST-RESULTS.txt"
set "FAILURES=0"

> "%RESULTS%" (
    echo RawMetal Automated Test Results
    echo Run: %STAMP%
    echo Executable: %ROOT%\RawMetal.exe
    echo.
)

echo Test output:
echo   %RUN_DIR%
echo.

call :run_test "Stalker animation + AI" "--stalker-test"
call :run_test "Physics + AI" "--physics-ai-test"
call :run_test "Save format + compatibility" "--save-test"
call :run_test "Full Vulkan smoke suite" "--smoke-test --vulkan"

rem Move any regular runtime logs generated in root during testing.
for %%F in (
    RawMetal-audio.txt
    RawMetal-renderer.txt
    RawMetal-error.txt
) do (
    if exist "%ROOT%\%%F" move /Y "%ROOT%\%%F" "%RUN_DIR%\%%F" >nul
)

if "!FAILURES!"=="0" (
    >> "%RESULTS%" echo.
    >> "%RESULTS%" echo RESULT: PASS
    echo.
    echo ============================================================
    echo   ALL AUTOMATED TESTS PASSED
    echo ============================================================
    echo Diagnostics:
    echo   %RUN_DIR%
    exit /b 0
)

>> "%RESULTS%" echo.
>> "%RESULTS%" echo RESULT: FAIL - !FAILURES! group(s)
echo.
echo ============================================================
echo   !FAILURES! TEST GROUP(S) FAILED
echo ============================================================
echo Results:
echo   %RESULTS%
exit /b 1

:run_test
set "TEST_NAME=%~1"
set "TEST_ARGS=%~2"

echo ------------------------------------------------------------
echo Running: !TEST_NAME!
echo Args   : !TEST_ARGS!

pushd "%RUN_DIR%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$p=Start-Process -FilePath '%ROOT%\RawMetal.exe' -ArgumentList '!TEST_ARGS!' -Wait -PassThru; exit $p.ExitCode"
set "CODE=!ERRORLEVEL!"
popd

>> "%RESULTS%" echo !TEST_NAME!: exit !CODE!

if "!CODE!"=="0" (
    echo [PASS] !TEST_NAME!
) else (
    echo [FAIL] !TEST_NAME! - exit !CODE!
    call :explain_code !CODE!
    set /a FAILURES+=1
)
echo.
exit /b 0

:explain_code
set "C=%~1"
set "MEANING=Unknown failure"
if "%C%"=="2"  set "MEANING=Final smoke frame/player-state failure"
if "%C%"=="3"  set "MEANING=Enemy unreachable"
if "%C%"=="4"  set "MEANING=Pickup unreachable"
if "%C%"=="5"  set "MEANING=Extraction unreachable"
if "%C%"=="7"  set "MEANING=3D renderer validation failed"
if "%C%"=="9"  set "MEANING=Combat test failed"
if "%C%"=="10" set "MEANING=Weapon attachment/grip test failed"
if "%C%"=="11" set "MEANING=Weapon motion test failed"
if "%C%"=="12" set "MEANING=Audio event test failed"
if "%C%"=="13" set "MEANING=Audio engine test failed"
if "%C%"=="14" set "MEANING=Map connectivity test failed"
if "%C%"=="15" set "MEANING=Audio device test failed"
if "%C%"=="16" set "MEANING=Settings test failed"
if "%C%"=="17" set "MEANING=Viewport test failed"
if "%C%"=="18" set "MEANING=Pickup test failed"
if "%C%"=="19" set "MEANING=Movement test failed"
if "%C%"=="20" set "MEANING=Progression test failed"
if "%C%"=="21" set "MEANING=AI test failed"
if "%C%"=="22" set "MEANING=Clutter/unarmed related test failed"
if "%C%"=="23" set "MEANING=Placement test failed"
if "%C%"=="24" set "MEANING=Gantry test failed"
if "%C%"=="25" set "MEANING=Clutter test failed"
if "%C%"=="26" set "MEANING=Streaming test failed"
if "%C%"=="31" set "MEANING=Inventory test failed"
if "%C%"=="32" set "MEANING=Lift test failed"
if "%C%"=="33" set "MEANING=Lift audio mix test failed"
if "%C%"=="34" set "MEANING=Console test failed"
if "%C%"=="35" set "MEANING=Performance test failed"
if "%C%"=="36" set "MEANING=Vulkan/hardware renderer test failed"
if "%C%"=="37" set "MEANING=Reactor test failed"
if "%C%"=="38" set "MEANING=Save/load test failed"
if "%C%"=="39" set "MEANING=Controls/settings test failed"
if "%C%"=="40" set "MEANING=Stalker animation/AI test failed"
if "%C%"=="41" set "MEANING=Hazmat test failed"

echo        !MEANING!
>> "%RESULTS%" echo     !MEANING!
exit /b 0

:sync_core
echo.
echo ============================================================
echo   GITHUB SYNC
echo ============================================================
echo.

if not exist "%ROOT%\SyncGitHub.cmd" (
    echo [ERROR] SyncGitHub.cmd is missing.
    exit /b 1
)

call "%ROOT%\SyncGitHub.cmd"
exit /b %ERRORLEVEL%

:ship_core
echo.
echo ============================================================
echo   SHIP RAWMETAL
echo ============================================================
echo.

if not exist "%ROOT%\ShipRawMetal.cmd" (
    echo [ERROR] ShipRawMetal.cmd is missing.
    exit /b 1
)

call "%ROOT%\ShipRawMetal.cmd"
exit /b %ERRORLEVEL%

:clean_core
echo.
echo ============================================================
echo   CLEAN GENERATED ROOT OUTPUT
echo ============================================================
echo.

for /f %%A in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH-mm-ss"') do set "STAMP=%%A"
set "CLEAN_DIR=%ROOT%\diagnostics\cleanup-%STAMP%"
set "MOVED=0"

for %%F in (
    RawMetal-audio.txt
    RawMetal-renderer.txt
    RawMetal-error.txt
    ai-diagnostic.txt
    ai-test.txt
    model-report.txt
    map-test.txt
    attachment-test.txt
    placement-test.txt
    render-benchmark.txt
) do (
    if exist "%ROOT%\%%F" (
        if "!MOVED!"=="0" mkdir "%CLEAN_DIR%" >nul 2>&1
        move /Y "%ROOT%\%%F" "%CLEAN_DIR%\%%F" >nul
        set "MOVED=1"
        echo [MOVED] %%F
    )
)

for %%F in ("%ROOT%\*.ppm") do (
    if exist "%%~fF" (
        if "!MOVED!"=="0" mkdir "%CLEAN_DIR%" >nul 2>&1
        move /Y "%%~fF" "%CLEAN_DIR%\%%~nxF" >nul
        set "MOVED=1"
        echo [MOVED] %%~nxF
    )
)

if "!MOVED!"=="0" (
    echo [OK] Root is already clean.
) else (
    echo.
    echo [OK] Generated files moved to:
    echo   %CLEAN_DIR%
)

exit /b 0

:status_core
echo.
echo ============================================================
echo   RAWMETAL STATUS
echo ============================================================
echo.

where git >nul 2>&1
if not errorlevel 1 (
    git rev-parse --is-inside-work-tree >nul 2>&1
    if not errorlevel 1 (
        echo Git:
        echo ------------------------------------------------------------
        git status --short --branch --untracked-files=all
        echo ------------------------------------------------------------
        echo.
    )
)

if exist "%ROOT%\RawMetal.exe" (
    for %%A in ("%ROOT%\RawMetal.exe") do (
        set "EXE_BYTES=%%~zA"
        set "EXE_TIME=%%~tA"
    )
    echo RawMetal.exe : !EXE_BYTES! bytes
    echo Built        : !EXE_TIME!
) else (
    echo RawMetal.exe : MISSING
)

if exist "%ROOT%\RawMetal.zip" (
    for %%A in ("%ROOT%\RawMetal.zip") do set "ZIP_BYTES=%%~zA"
    echo RawMetal.zip : !ZIP_BYTES! bytes
) else (
    echo RawMetal.zip : not present
)

echo.
if exist "%ROOT%\diagnostics" (
    set "LATEST="
    for /f "delims=" %%D in ('dir /b /ad /o-d "%ROOT%\diagnostics" 2^>nul') do (
        if not defined LATEST set "LATEST=%%D"
    )
    if defined LATEST (
        echo Latest diagnostics:
        echo   %ROOT%\diagnostics\!LATEST!
    ) else (
        echo Diagnostics folder exists but has no run folders.
    )
) else (
    echo Diagnostics folder does not exist yet.
)

exit /b 0

:pause_menu
echo.
pause
exit /b 0
