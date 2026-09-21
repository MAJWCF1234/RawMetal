@echo off
setlocal
cd /d "%~dp0"

echo Starting Depthworks Level Editor...
where py >nul 2>nul
if not errorlevel 1 (
    py -3 tools\level-editor\server.py
    exit /b %errorlevel%
)

where python >nul 2>nul
if not errorlevel 1 (
    python tools\level-editor\server.py
    exit /b %errorlevel%
)

echo.
echo Python 3 was not found in PATH.
echo Install Python 3 or make the py/python launcher available, then run LevelEditor.cmd again.
pause
exit /b 1
