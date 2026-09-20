@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RawMetal GitHub Sync v2
cd /d "%~dp0"

set "TARGET_BRANCH=main"
set "EXPECTED_REMOTE=MAJWCF1234/RawMetal"

call :check_repo
if errorlevel 1 goto :fatal

:menu
cls
echo ============================================================
echo   RawMetal GitHub Sync v2
echo ============================================================
echo.
echo Repository : %ROOT%
echo Branch     : %BRANCH%
echo Origin     : %ORIGIN%
echo.
echo [1] INSTALL FROM GITHUB
echo     Bring this PC up to date with origin/main.
echo     Local tracked edits are preserved with autostash.
echo.
echo [2] SYNC TO GITHUB
echo     Commit and push ALL non-ignored local changes.
echo     This includes real file deletions.
echo.
echo [3] SYNC BOTH
echo     Install GitHub changes first, then upload local changes.
echo.
echo [4] STATUS
echo.
echo [Q] QUIT
echo.
choice /C 1234Q /N /M "Choose: "
if errorlevel 5 exit /b 0
if errorlevel 4 goto :status
if errorlevel 3 goto :both
if errorlevel 2 goto :push_menu
if errorlevel 1 goto :pull_menu
goto :menu

:status
call :fetch
if errorlevel 1 goto :pause_menu
call :counts
echo.
echo ------------------------------------------------------------
git status --short --branch --untracked-files=all
echo ------------------------------------------------------------
echo.
echo Local commits ahead : !AHEAD!
echo GitHub commits ahead: !BEHIND!
echo.
pause
goto :menu

:pull_menu
call :pull_core
echo.
pause
goto :menu

:push_menu
call :push_core
echo.
pause
goto :menu

:both
echo.
echo ============================================================
echo   SYNC BOTH DIRECTIONS
echo ============================================================
echo.
call :pull_core
if errorlevel 1 (
    echo.
    echo [STOP] Upload skipped because the GitHub install step failed.
    echo.
    pause
    goto :menu
)

call :push_core
echo.
pause
goto :menu

:pull_core
echo.
echo ============================================================
echo   INSTALL FROM GITHUB
echo ============================================================
echo.

call :fetch
if errorlevel 1 exit /b 1
call :counts

echo Local commits ahead : !AHEAD!
echo GitHub commits ahead: !BEHIND!
echo.

if "!BEHIND!"=="0" (
    echo [OK] This PC already has the newest GitHub commit.
    if not "!AHEAD!"=="0" echo [INFO] You still have !AHEAD! local commit or commits not pushed.
    exit /b 0
)

if not "!AHEAD!"=="0" (
    echo [INFO] Local and remote history both have commits.
    echo        Git will rebase the local commits onto the newest GitHub main.
    echo.
)

echo Current tracked edits will be temporarily autostashed by Git.
echo Untracked files stay where they are.
echo.
choice /C YN /N /M "Install !BEHIND! GitHub commit or commits now? [Y/N] "
if errorlevel 2 (
    echo [CANCELLED] Nothing changed.
    exit /b 0
)

git pull --rebase --autostash origin %TARGET_BRANCH%
if errorlevel 1 (
    echo.
    echo [STOP] Git needs manual attention.
    echo Run: git status
    echo.
    echo If a rebase conflict is shown, resolve it and run:
    echo   git rebase --continue
    echo.
    echo To cancel the rebase:
    echo   git rebase --abort
    exit /b 1
)

echo.
echo [OK] GitHub edits are installed locally.
echo.
git status --short --branch
exit /b 0

:push_core
echo.
echo ============================================================
echo   SYNC TO GITHUB
echo ============================================================
echo.

call :fetch
if errorlevel 1 exit /b 1
call :counts

if not "!BEHIND!"=="0" (
    echo [STOP] GitHub has !BEHIND! newer commit or commits.
    echo        Install FROM GitHub first.
    exit /b 1
)

echo Staging all non-ignored changes so local and GitHub can mirror each other...
git add -A
if errorlevel 1 exit /b 1

echo.
echo Changes ready for GitHub:
echo ------------------------------------------------------------
git diff --cached --name-status
echo ------------------------------------------------------------
echo.

git diff --cached --quiet
if not errorlevel 1 (
    call :counts
    if "!AHEAD!"=="0" (
        echo [OK] Nothing needs uploading.
        exit /b 0
    )

    echo [INFO] No new working-tree changes, but !AHEAD! local commit or commits need pushing.
    choice /C YN /N /M "Push existing local commits now? [Y/N] "
    if errorlevel 2 (
        echo [CANCELLED] Nothing pushed.
        exit /b 0
    )

    git push origin %TARGET_BRANCH%
    if errorlevel 1 exit /b 1
    echo [OK] Local commits pushed to GitHub.
    exit /b 0
)

git diff --cached --check
if errorlevel 1 (
    echo.
    echo [STOP] Git found a whitespace or patch-format problem.
    echo        Nothing was committed.
    exit /b 1
)

echo IMPORTANT:
echo The list above is the exact Git change set.
echo Deleted files shown with D WILL be deleted from GitHub.
echo New files shown with A WILL be added.
echo Modified files shown with M WILL be updated.
echo.
choice /C YN /N /M "Commit exactly this change set? [Y/N] "
if errorlevel 2 (
    echo.
    echo [CANCELLED] Nothing was committed.
    echo Changes remain staged so you can inspect them.
    exit /b 0
)

set "COMMIT_MSG="
set /p "COMMIT_MSG=Commit message [Sync RawMetal]: "
if not defined COMMIT_MSG set "COMMIT_MSG=Sync RawMetal"

git commit -m "%COMMIT_MSG%"
if errorlevel 1 exit /b 1

echo.
echo Pushing origin/%TARGET_BRANCH%...
git push origin %TARGET_BRANCH%
if errorlevel 1 (
    echo.
    echo [STOP] Commit succeeded locally but push failed.
    echo Your commit is safe on this PC.
    exit /b 1
)

echo.
echo [OK] Local changes are synced to GitHub.
exit /b 0

:fetch
echo Fetching origin/%TARGET_BRANCH%...
git fetch origin
if errorlevel 1 (
    echo [ERROR] Could not fetch GitHub.
    exit /b 1
)
exit /b 0

:counts
set "AHEAD=0"
set "BEHIND=0"
for /f "delims=" %%A in ('git rev-list --count origin/%TARGET_BRANCH%..HEAD') do set "AHEAD=%%A"
for /f "delims=" %%A in ('git rev-list --count HEAD..origin/%TARGET_BRANCH%') do set "BEHIND=%%A"
exit /b 0

:check_repo
where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git is not installed or is not in PATH.
    exit /b 1
)

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Put this CMD in the RawMetal repository root.
    exit /b 1
)

for /f "delims=" %%A in ('git rev-parse --show-toplevel') do set "ROOT=%%A"
cd /d "%ROOT%"

for /f "delims=" %%A in ('git branch --show-current') do set "BRANCH=%%A"
if /i not "%BRANCH%"=="%TARGET_BRANCH%" (
    echo [ERROR] Current branch is "%BRANCH%".
    echo Expected branch is "%TARGET_BRANCH%".
    exit /b 1
)

for /f "delims=" %%A in ('git remote get-url origin 2^>nul') do set "ORIGIN=%%A"
if not defined ORIGIN (
    echo [ERROR] No Git remote named origin exists.
    exit /b 1
)

echo %ORIGIN% | findstr /I /C:"%EXPECTED_REMOTE%" >nul
if errorlevel 1 (
    echo [ERROR] Origin does not look like the RawMetal repository.
    echo Origin: %ORIGIN%
    exit /b 1
)

exit /b 0

:pause_menu
echo.
pause
goto :menu

:fatal
echo.
echo ============================================================
echo   RawMetal GitHub Sync could not start
echo ============================================================
echo.
pause
exit /b 1
