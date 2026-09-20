@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal Ship - source + one-file friend release
cd /d "%~dp0"

rem ============================================================
rem RawMetal Ship
rem Publishes:
rem   1) current approved SOURCE changes to GitHub main
rem   2) ONE release asset: RawMetal.exe
rem GitHub automatically provides Source code (zip/tar.gz) for the tag.
rem
rem It deliberately ignores unrelated deleted docs locally:
rem they are NOT restored, NOT staged, and NOT committed.
rem ============================================================

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
    echo [ERROR] Put this CMD in E:\rawmetal and run it there.
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
    echo [INFO] !DELETED_COUNT! tracked docs/source-note files are deleted locally.
    echo        This ship script will NOT restore them and will NOT commit those deletions.
    echo        Their existing copies remain on GitHub.
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
        ) else (
            echo [OK] %%~F
        )
    )
)
if "!ASSET_ERROR!"=="1" goto :fail
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
    echo [STOP] RawMetal.exe is not under 19,000,000 bytes.
    echo        Nothing was committed or released.
    goto :fail
)
echo [OK] Friend build is under 19 MB.
echo.

echo [5/7] Staging SOURCE changes only...
echo        Allowed files:
echo          src/CMakeLists.txt
echo          src/game/AI.cpp
echo          src/game/Game.h
echo          src/game/Saves.cpp
echo.
git add -- src/CMakeLists.txt src/game/AI.cpp src/game/Game.h src/game/Saves.cpp
if errorlevel 1 goto :fail

git diff --cached --check
if errorlevel 1 (
    echo [ERROR] Staged source failed git diff --check.
    git reset -- src/CMakeLists.txt src/game/AI.cpp src/game/Game.h src/game/Saves.cpp
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
set /p "COMMIT_MSG=Commit message [Finish Stalker AI and save integration]: "
if not defined COMMIT_MSG set "COMMIT_MSG=Finish Stalker AI and save integration"

git commit -m "%COMMIT_MSG%"
if errorlevel 1 goto :fail

git push origin %TARGET_BRANCH%
if errorlevel 1 (
    echo [ERROR] Push failed. Commit still exists locally.
    goto :fail
)

:source_synced
echo [OK] Source is synced to GitHub main.
echo.

echo [6/7] Preparing GitHub Release...
where gh >nul 2>&1
if errorlevel 1 (
    echo [STOP] GitHub CLI "gh" is not installed.
    echo        Source IS synced, but the release asset was not uploaded.
    echo        Install GitHub CLI, run "gh auth login", then rerun this CMD.
    goto :done
)

gh auth status >nul 2>&1
if errorlevel 1 (
    echo [STOP] GitHub CLI is installed but not authenticated.
    echo        Run: gh auth login
    echo        Then rerun this CMD.
    goto :done
)

set "TAG="
set /p "TAG=Release tag [v0.3.7]: "
if not defined TAG set "TAG=v0.3.7"

gh release view "%TAG%" --repo "%EXPECTED_REPO%" >nul 2>&1
if not errorlevel 1 (
    echo [STOP] Release/tag "%TAG%" already exists.
    goto :done
)

echo.
echo This will publish ONE downloadable friend file:
echo   RawMetal.exe  (!EXE_BYTES! bytes)
echo.
echo GitHub will ALSO automatically show its normal:
echo   Source code (zip)
echo   Source code (tar.gz)
echo for the same tag.
echo.
choice /C YN /N /M "Publish release %TAG% now? [Y/N] "
if errorlevel 2 goto :done

echo [7/7] Publishing release...
gh release create "%TAG%" "RawMetal.exe" ^
    --repo "%EXPECTED_REPO%" ^
    --target "%TARGET_BRANCH%" ^
    --title "RawMetal %TAG%" ^
    --generate-notes

if errorlevel 1 (
    echo [ERROR] Release creation failed.
    goto :done
)

echo.
echo ============================================================
echo   SHIPPED
echo   Source       : GitHub main
echo   Friend build : RawMetal.exe (!EXE_BYTES! bytes)
echo   Release      : %TAG%
echo ============================================================
echo.
goto :done

:done
echo.
echo Local deleted docs were left alone and were not published as deletions.
echo.
pause
exit /b 0

:fail
echo.
echo ============================================================
echo   STOPPED SAFELY
echo ============================================================
echo Nothing unrelated was restored, staged, committed, or released.
echo.
pause
exit /b 1
