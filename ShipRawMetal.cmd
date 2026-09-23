@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal Ship - source + one-file friend release
cd /d "%~dp0"

set "TARGET_BRANCH=main"
set "EXPECTED_REPO=MAJWCF1234/RawMetal"
set "MAX_BYTES=19000000"

echo.
echo ============================================================
echo   RawMetal Ship - source + one-file friend release
echo ============================================================
echo.

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git is not installed or not in PATH.
    goto :fail
)

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Run this script from the repository root.
    goto :fail
)

for /f "delims=" %%A in ('git rev-parse --show-toplevel') do set "ROOT=%%A"
cd /d "%ROOT%"

for /f "delims=" %%A in ('git branch --show-current') do set "BRANCH=%%A"
if /i not "%BRANCH%"=="%TARGET_BRANCH%" (
    echo [ERROR] Current branch is "%BRANCH%"; expected "%TARGET_BRANCH%".
    goto :fail
)

for /f "delims=" %%A in ('git remote get-url origin 2^>nul') do set "ORIGIN=%%A"
if not defined ORIGIN (
    echo [ERROR] No origin remote is configured.
    goto :fail
)

echo [OK] Repository : %ROOT%
echo [OK] Branch     : %BRANCH%
echo [OK] Origin     : %ORIGIN%
echo.

echo [1/7] Fetching GitHub...
git fetch origin
if errorlevel 1 goto :fail

for /f "delims=" %%A in ('git rev-list --count HEAD..origin/%TARGET_BRANCH%') do set "BEHIND=%%A"
if not "%BEHIND%"=="0" (
    echo [STOP] GitHub main is %BEHIND% commits ahead of this PC.
    echo        Pull/reconcile first so nothing gets overwritten.
    goto :fail
)
echo [OK] Local branch is not behind GitHub.
echo.

echo [2/7] Ignoring unrelated local deletions...
set "DELETED_COUNT=0"
for /f "usebackq delims=" %%F in (`git ls-files -d`) do set /a DELETED_COUNT+=1
if not "!DELETED_COUNT!"=="0" (
    echo [INFO] !DELETED_COUNT! tracked docs/notes are deleted locally.
    echo        Left alone; existing copies remain on GitHub.
) else (
    echo [OK] No unrelated tracked deletions.
)
echo.

echo [3/7] Verifying embedded Stalker assets...
set "ASSET_ERROR=0"
for %%F in (
    "src\assets\reactor\stalker.obj"
    "src\assets\reactor\stalker.png"
    "src\assets\reactor\stalker-animation.bin"
) do (
    if not exist "%%~F" (
        echo [ERROR] Missing: %%~F
        set "ASSET_ERROR=1"
    ) else (
        git ls-files --error-unmatch "%%~F" >nul 2>&1
        if errorlevel 1 (
            echo [ERROR] Exists but is not tracked by Git: %%~F
            set "ASSET_ERROR=1"
        )
    )
)
if "!ASSET_ERROR!"=="1" goto :fail
echo [OK] Assets verified.
echo.

echo [4/7] Building release EXE...
if not exist "Build.cmd" (
    echo [ERROR] Build.cmd was not found.
    goto :fail
)
call Build.cmd
if errorlevel 1 (
    echo [ERROR] Build failed. Nothing will be published.
    goto :fail
)
if not exist "RawMetal.exe" (
    echo [ERROR] Build finished but RawMetal.exe does not exist.
    goto :fail
)

for %%A in ("RawMetal.exe") do set "EXE_BYTES=%%~zA"
echo [INFO] RawMetal.exe = !EXE_BYTES! bytes
if !EXE_BYTES! GEQ %MAX_BYTES% (
    echo [STOP] RawMetal.exe exceeds 19 MB (!EXE_BYTES! bytes^).
    goto :fail
)
echo [OK] Friend build is under 19 MB.
echo.

echo [5/7] Staging APPROVED source changes...
:: Added src/world files so map and engine edits actually get committed!
set "APPROVED_FILES=src/CMakeLists.txt src/game/AI.cpp src/game/Game.h src/game/Saves.cpp src/world/World.cpp src/world/World.h src/world/WorldDefinition.h"

git add -- %APPROVED_FILES% 2>nul

git diff --cached --check
if errorlevel 1 (
    echo [ERROR] Staged source failed git diff --check.
    git reset -- %APPROVED_FILES%
    goto :fail
)

git diff --cached --quiet
if not errorlevel 1 (
    echo [INFO] No approved source changes need a new commit.
    goto :source_synced
)

echo.
git diff --cached --stat
echo.
choice /C YN /N /M "Commit these source changes to GitHub main? [Y/N] "
if errorlevel 2 goto :fail

set "COMMIT_MSG="
set /p "COMMIT_MSG=Commit message [Update engine and map content]: "
if not defined COMMIT_MSG set "COMMIT_MSG=Update engine and map content"

git commit -m "%COMMIT_MSG%"
if errorlevel 1 goto :fail

git push origin %TARGET_BRANCH%
if errorlevel 1 (
    echo [ERROR] Push failed.
    goto :fail
)

:source_synced
echo [OK] Source is synced to GitHub main.
echo.

echo [6/7] Preparing GitHub Release...
where gh >nul 2>&1
if errorlevel 1 (
    echo [STOP] GitHub CLI "gh" is not installed.
    goto :done
)

gh auth status >nul 2>&1
if errorlevel 1 (
    echo [STOP] GitHub CLI is not authenticated. Run "gh auth login".
    goto :done
)

:: Automatically find latest git tag
set "LAST_TAG=v0.3.7"
for /f "delims=" %%T in ('git describe --tags --abbrev^=0 2^>nul') do set "LAST_TAG=%%T"

echo [INFO] Latest release tag was: %LAST_TAG%
set "TAG="
set /p "TAG=New release tag (e.g. v0.3.8): "
if not defined TAG (
    echo [ERROR] Tag cannot be empty.
    goto :done
)

gh release view "%TAG%" --repo "%EXPECTED_REPO%" >nul 2>&1
if not errorlevel 1 (
    echo [STOP] Release "%TAG%" already exists on GitHub!
    goto :done
)

echo.
echo Publishing friend build: RawMetal.exe (!EXE_BYTES! bytes) to %TAG%
choice /C YN /N /M "Publish release %TAG% now? [Y/N] "
if errorlevel 2 goto :done

echo [7/7] Publishing release asset...
set "NOTE_FLAG=--generate-notes"
if exist "RELEASE_NOTES.md" set "NOTE_FLAG=--notes-file RELEASE_NOTES.md"

gh release create "%TAG%" "RawMetal.exe" ^
    --repo "%EXPECTED_REPO%" ^
    --target "%TARGET_BRANCH%" ^
    --title "RawMetal %TAG%" ^
    !NOTE_FLAG!

if errorlevel 1 (
    echo [ERROR] Release creation failed.
    goto :done
)

echo.
echo ============================================================
echo   SHIPPED SUCCESSFULLY!
echo   Target : https://github.com/%EXPECTED_REPO%/releases/tag/%TAG%
echo ============================================================
goto :done

:done
pause
exit /b 0

:fail
echo.
echo ============================================================
echo   STOPPED SAFELY - Nothing published.
echo ============================================================
pause
exit /b 1