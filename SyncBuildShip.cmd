@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal Sync + Build + Ship
cd /d "%~dp0"

set "TARGET_BRANCH=main"
set "EXPECTED_REPO=MAJWCF1234/RawMetal"
set "MAX_BYTES=19000000"
set "ROOT=%CD%"

echo.
echo ============================================================
echo   RAWMETAL - SYNC + BUILD + SHIP
echo ============================================================
echo.
echo This pipeline will:
echo   1. Sync FROM GitHub
echo   2. Build Release
echo   3. Run the full Vulkan smoke test in diagnostics
echo   4. Sync ALL non-ignored local changes TO GitHub
echo   5. Publish RawMetal.exe as a GitHub release
echo.
echo Nothing is force-pushed.
echo.

rem ------------------------------------------------------------
rem REQUIREMENTS / REPO CHECK
rem ------------------------------------------------------------

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git is not installed or not in PATH.
    goto :fail
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake is not installed or not in PATH.
    goto :fail
)

where gh >nul 2>&1
if errorlevel 1 (
    echo [ERROR] GitHub CLI "gh" is not installed.
    goto :fail
)

gh auth status >nul 2>&1
if errorlevel 1 (
    echo [ERROR] GitHub CLI is not authenticated.
    echo Run: gh auth login
    goto :fail
)

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Put this CMD in the RawMetal repository root.
    goto :fail
)

for /f "delims=" %%A in ('git rev-parse --show-toplevel') do set "ROOT=%%A"
cd /d "%ROOT%"

for /f "delims=" %%A in ('git branch --show-current') do set "BRANCH=%%A"
if /I not "!BRANCH!"=="%TARGET_BRANCH%" (
    echo [ERROR] Current branch is "!BRANCH!".
    echo Expected: %TARGET_BRANCH%
    goto :fail
)

for /f "delims=" %%A in ('git remote get-url origin 2^>nul') do set "ORIGIN=%%A"
if not defined ORIGIN (
    echo [ERROR] No Git remote named origin exists.
    goto :fail
)

echo !ORIGIN! | findstr /I /C:"%EXPECTED_REPO%" >nul
if errorlevel 1 (
    echo [ERROR] Origin does not look like %EXPECTED_REPO%.
    echo Origin: !ORIGIN!
    goto :fail
)

echo [OK] Repository : %ROOT%
echo [OK] Branch     : !BRANCH!
echo [OK] Origin     : !ORIGIN!
echo.

set "TAG="
set /p "TAG=Release tag, for example v0.3.8: "
if not defined TAG (
    echo [ERROR] A release tag is required.
    goto :fail
)

gh release view "!TAG!" --repo "%EXPECTED_REPO%" >nul 2>&1
if not errorlevel 1 (
    echo [ERROR] Release "!TAG!" already exists.
    goto :fail
)

echo.
choice /C YN /N /M "Sync, build, test, and ship !TAG!? [Y/N] "
if errorlevel 2 (
    echo [CANCELLED] Nothing changed.
    exit /b 0
)

rem ------------------------------------------------------------
rem 1. SYNC FROM GITHUB
rem ------------------------------------------------------------

echo.
echo ============================================================
echo   [1/5] SYNC FROM GITHUB
echo ============================================================
echo.

git fetch origin
if errorlevel 1 goto :fail

set "AHEAD=0"
set "BEHIND=0"
for /f "delims=" %%A in ('git rev-list --count origin/%TARGET_BRANCH%..HEAD') do set "AHEAD=%%A"
for /f "delims=" %%A in ('git rev-list --count HEAD..origin/%TARGET_BRANCH%') do set "BEHIND=%%A"

echo Local commits ahead : !AHEAD!
echo GitHub commits ahead: !BEHIND!
echo.

if not "!BEHIND!"=="0" (
    echo Installing GitHub changes with rebase + autostash...
    git pull --rebase --autostash origin %TARGET_BRANCH%
    if errorlevel 1 (
        echo.
        echo [STOP] Git sync needs manual attention.
        echo Run: git status
        goto :fail
    )
) else (
    echo [OK] Local copy already contains the newest GitHub commit.
)

rem ------------------------------------------------------------
rem 2. BUILD
rem ------------------------------------------------------------

echo.
echo ============================================================
echo   [2/5] BUILD RELEASE
echo ============================================================
echo.

if not exist "%ROOT%\Build.cmd" (
    echo [ERROR] Build.cmd is missing.
    goto :fail
)

call "%ROOT%\Build.cmd"
if errorlevel 1 (
    echo [ERROR] Build failed. Nothing will be committed or released.
    goto :fail
)

if not exist "%ROOT%\RawMetal.exe" (
    echo [ERROR] Build completed but RawMetal.exe is missing.
    goto :fail
)

for %%A in ("%ROOT%\RawMetal.exe") do set "EXE_BYTES=%%~zA"

echo.
echo RawMetal.exe: !EXE_BYTES! bytes

if !EXE_BYTES! GEQ %MAX_BYTES% (
    echo [STOP] RawMetal.exe is not under 19,000,000 bytes.
    echo Nothing will be committed or released.
    goto :fail
)

echo [OK] Friend build is under the 19 MB ceiling.

rem ------------------------------------------------------------
rem 3. RELEASE TEST
rem ------------------------------------------------------------

echo.
echo ============================================================
echo   [3/5] FULL VULKAN SMOKE TEST
echo ============================================================
echo.

for /f %%A in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH-mm-ss"') do set "STAMP=%%A"
set "RUN_DIR=%ROOT%\diagnostics\ship-!STAMP!"

mkdir "!RUN_DIR!" >nul 2>&1
if not exist "!RUN_DIR!" (
    echo [ERROR] Could not create diagnostics folder.
    goto :fail
)

echo All test images and logs stay inside:
echo   !RUN_DIR!
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$p=Start-Process -FilePath '%ROOT%\RawMetal.exe' -ArgumentList @('--smoke-test','--vulkan') -WorkingDirectory '!RUN_DIR!' -Wait -PassThru; exit $p.ExitCode"

set "TEST_CODE=!ERRORLEVEL!"

if not "!TEST_CODE!"=="0" (
    echo.
    echo [STOP] Vulkan smoke test failed with exit code !TEST_CODE!.
    echo Diagnostics:
    echo   !RUN_DIR!
    echo Nothing will be committed or released.
    goto :fail
)

echo [OK] Full Vulkan smoke test passed.

rem ------------------------------------------------------------
rem 4. SYNC TO GITHUB
rem ------------------------------------------------------------

echo.
echo ============================================================
echo   [4/5] SYNC TO GITHUB
echo ============================================================
echo.

git fetch origin
if errorlevel 1 goto :fail

for /f "delims=" %%A in ('git rev-list --count HEAD..origin/%TARGET_BRANCH%') do set "BEHIND=%%A"
if not "!BEHIND!"=="0" (
    echo [STOP] GitHub changed while the build was being tested.
    echo Re-run this CMD so the newer GitHub state is tested too.
    goto :fail
)

git add -A
if errorlevel 1 goto :fail

git diff --cached --check
if errorlevel 1 (
    echo.
    echo [STOP] Staged changes failed git diff --check.
    echo Nothing was committed.
    goto :fail
)

echo.
echo Exact source/repo changes that will be committed:
echo ------------------------------------------------------------
git diff --cached --name-status
echo ------------------------------------------------------------
echo.

git diff --cached --quiet
if errorlevel 1 (
    choice /C YN /N /M "Commit exactly these changes for !TAG!? [Y/N] "
    if errorlevel 2 (
        echo [CANCELLED] Changes remain staged. Nothing was pushed or released.
        exit /b 0
    )

    git commit -m "Sync and ship !TAG!"
    if errorlevel 1 goto :fail
) else (
    echo [INFO] No new working-tree changes need a commit.
)

for /f "delims=" %%A in ('git rev-list --count origin/%TARGET_BRANCH%..HEAD') do set "AHEAD=%%A"
if not "!AHEAD!"=="0" (
    echo Pushing !AHEAD! local commit or commits...
    git push origin %TARGET_BRANCH%
    if errorlevel 1 (
        echo [ERROR] Push failed. Local commits are safe on this PC.
        goto :fail
    )
) else (
    echo [OK] GitHub main already matches the tested source state.
)

for /f "delims=" %%A in ('git rev-parse HEAD') do set "HEAD_SHA=%%A"

rem ------------------------------------------------------------
rem 5. SHIP RELEASE
rem ------------------------------------------------------------

echo.
echo ============================================================
echo   [5/5] PUBLISH RELEASE !TAG!
echo ============================================================
echo.

gh release view "!TAG!" --repo "%EXPECTED_REPO%" >nul 2>&1
if not errorlevel 1 (
    echo [STOP] Release "!TAG!" now exists. Refusing to overwrite it.
    goto :fail
)

for /f "delims=" %%A in ('powershell -NoProfile -Command "(Get-FileHash '%ROOT%\RawMetal.exe' -Algorithm SHA256).Hash"') do set "EXE_SHA256=%%A"

echo Commit : !HEAD_SHA!
echo EXE    : !EXE_BYTES! bytes
echo SHA256 : !EXE_SHA256!
echo.

gh release create "!TAG!" "%ROOT%\RawMetal.exe" ^
    --repo "%EXPECTED_REPO%" ^
    --target "!HEAD_SHA!" ^
    --title "RawMetal !TAG!" ^
    --generate-notes

if errorlevel 1 (
    echo.
    echo [ERROR] Source was pushed, but GitHub release creation failed.
    echo The tested commit remains safely on GitHub.
    goto :fail
)

> "!RUN_DIR!\SHIP-RESULT.txt" (
    echo RawMetal !TAG!
    echo Commit: !HEAD_SHA!
    echo RawMetal.exe: !EXE_BYTES! bytes
    echo SHA256: !EXE_SHA256!
    echo Smoke test exit code: !TEST_CODE!
)

echo.
echo ============================================================
echo   SHIPPED SUCCESSFULLY
echo ============================================================
echo.
echo Release : !TAG!
echo Commit  : !HEAD_SHA!
echo EXE     : !EXE_BYTES! bytes
echo SHA256  : !EXE_SHA256!
echo.
echo Diagnostics:
echo   !RUN_DIR!
echo.
pause
exit /b 0

:fail
echo.
echo ============================================================
echo   STOPPED SAFELY
echo ============================================================
echo.
echo A failed build/test is never released.
echo No force-push was attempted.
echo.
pause
exit /b 1
