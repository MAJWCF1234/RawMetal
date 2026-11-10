@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist ".build\native-ui" mkdir ".build\native-ui"
cl /nologo /utf-8 /std:c++20 /O2 /EHsc editor\tests\CorridorRouteRegression.cpp /Fo:.build\native-ui\CorridorRouteRegression.obj /Fe:.build\native-ui\CorridorRouteRegression.exe
if errorlevel 1 exit /b %errorlevel%
.build\native-ui\CorridorRouteRegression.exe > native-editor-corridor-route-test.txt 2>&1
set "corridor_result=%errorlevel%"
type native-editor-corridor-route-test.txt
exit /b %corridor_result%
