@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal Ship - one-file release
cd /d "%~dp0"

rem ============================================================
rem Publishes the committed source as a GitHub release with
rem RawMetal.exe attached. It does not commit anything: commit and
rem push with SyncGitHub.cmd (or git) first.
rem ============================================================

set "TARGET_BRANCH=main"
set "EXPECTED_REPO=MAJWCF1234/RawMetal"
rem Same ceiling the build enforces in src/tools/check_size.cmake.
set "MAX_BYTES=33554432"

echo.
echo ============================================================
echo   RawMetal Ship - one-file release
echo ============================================================
echo.

where git >nul 2>&1 || (echo [ERROR] Git is not installed or not in PATH.& goto :fail)
git rev-parse --is-inside-work-tree >nul 2>&1 || (echo [ERROR] Run this script from the repository.& goto :fail)

for /f "delims=" %%A in ('git rev-parse --show-toplevel') do set "ROOT=%%A"
cd /d "%ROOT%"

for /f "delims=" %%A in ('git branch --show-current') do set "BRANCH=%%A"
if /i not "%BRANCH%"=="%TARGET_BRANCH%" (
    echo [ERROR] Current branch is "%BRANCH%"; expected "%TARGET_BRANCH%".
    goto :fail
)

echo [1/5] Checking the source matches GitHub...
git fetch origin || goto :fail
set "AHEAD=0"
set "BEHIND=0"
for /f "delims=" %%A in ('git rev-list --count origin/%TARGET_BRANCH%..HEAD') do set "AHEAD=%%A"
for /f "delims=" %%A in ('git rev-list --count HEAD..origin/%TARGET_BRANCH%') do set "BEHIND=%%A"
if not "!BEHIND!"=="0" (
    echo [STOP] GitHub has !BEHIND! newer commit^(s^). Sync from GitHub first.
    goto :fail
)
if not "!AHEAD!"=="0" (
    echo [STOP] !AHEAD! local commit^(s^) are not on GitHub. Push them first.
    goto :fail
)
set "DIRTY="
for /f "delims=" %%A in ('git status --porcelain --untracked-files^=no') do set "DIRTY=1"
if defined DIRTY (
    echo [STOP] Tracked files have uncommitted changes. Commit or stash them so
    echo        the release matches the source on GitHub.
    git status --short --untracked-files=no
    goto :fail
)
echo [OK] HEAD matches origin/%TARGET_BRANCH%.
for /f "delims=" %%A in ('git rev-parse HEAD') do set "HEAD_SHA=%%A"
git fetch origin --tags >nul 2>&1
set "SHIPPED_TAG="
for /f "delims=" %%T in ('git tag --points-at HEAD') do set "SHIPPED_TAG=%%T"
if defined SHIPPED_TAG (
    echo [STOP] This commit is already released as !SHIPPED_TAG!. Nothing new to ship.
    goto :fail
)
echo.

echo [2/5] Building release EXE...
rem Skip the build when RawMetal.exe was already built from this exact commit
rem with no uncommitted changes; Build.cmd records that in built-from.txt.
set "BUILT_FROM="
if exist "%ROOT%\.build\built-from.txt" set /p "BUILT_FROM="<"%ROOT%\.build\built-from.txt"
if exist "RawMetal.exe" if "!BUILT_FROM!"=="!HEAD_SHA! clean" (
    echo [OK] RawMetal.exe is already built from !HEAD_SHA!; skipping the build.
    goto :built
)
call "%ROOT%\Build.cmd"
if errorlevel 1 (
    echo [ERROR] Build failed. Nothing will be published.
    goto :fail
)
:built
if not exist "RawMetal.exe" (
    echo [ERROR] Build finished but RawMetal.exe does not exist.
    goto :fail
)
for %%A in ("RawMetal.exe") do set "EXE_BYTES=%%~zA"
if !EXE_BYTES! GEQ %MAX_BYTES% (
    echo [STOP] RawMetal.exe is !EXE_BYTES! bytes; the ceiling is %MAX_BYTES% bytes ^(32 MiB^).
    goto :fail
)
set /a HEADROOM=%MAX_BYTES%-!EXE_BYTES!
echo [OK] RawMetal.exe = !EXE_BYTES! bytes ^(!HEADROOM! bytes under 32 MiB^).
echo.

echo [3/5] Checking GitHub CLI...
where gh >nul 2>&1 || (echo [STOP] GitHub CLI "gh" is not installed.& goto :fail)
gh auth status >nul 2>&1 || (echo [STOP] GitHub CLI is not signed in. Run "gh auth login".& goto :fail)
echo [OK] gh is ready.
echo.

echo [4/5] Choosing a tag...
set "LAST_TAG=(none)"
for /f "delims=" %%T in ('git describe --tags --abbrev^=0 2^>nul') do set "LAST_TAG=%%T"
echo [INFO] Nearest existing tag: !LAST_TAG!
set "TAG="
set /p "TAG=New release tag (e.g. v0.7.2-preview.14): "
if not defined TAG (
    echo [ERROR] Tag cannot be empty.
    goto :fail
)
gh release view "%TAG%" --repo "%EXPECTED_REPO%" >nul 2>&1
if not errorlevel 1 (
    echo [STOP] Release "%TAG%" already exists on GitHub.
    goto :fail
)
for /f "delims=" %%A in ('git rev-parse HEAD') do set "HEAD_SHA=%%A"
echo.
echo Publishing RawMetal.exe ^(!EXE_BYTES! bytes^) as %TAG% at !HEAD_SHA!
choice /C YN /N /M "Publish release %TAG% now? [Y/N] "
if errorlevel 2 goto :fail

echo [5/5] Publishing release...
set "NOTE_FLAG=--generate-notes"
if exist "RELEASE_NOTES.md" set "NOTE_FLAG=--notes-file RELEASE_NOTES.md"
gh release create "%TAG%" "RawMetal.exe" --repo "%EXPECTED_REPO%" --target "!HEAD_SHA!" --title "RawMetal %TAG%" !NOTE_FLAG!
if errorlevel 1 (
    echo [ERROR] Release creation failed.
    goto :fail
)

echo.
echo ============================================================
echo   SHIPPED: https://github.com/%EXPECTED_REPO%/releases/tag/%TAG%
echo ============================================================
pause
exit /b 0

:fail
echo.
echo ============================================================
echo   STOPPED - nothing was published.
echo ============================================================
pause
exit /b 1
