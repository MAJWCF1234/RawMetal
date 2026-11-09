@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist ".build\native-ui" mkdir ".build\native-ui"
cl /nologo /utf-8 /std:c++20 /O2 /EHsc editor\tests\CreationRegression.cpp /Fo:.build\native-ui\CreationRegression.obj /Fe:.build\native-ui\CreationRegression.exe /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib
if errorlevel 1 exit /b %errorlevel%
.build\native-ui\CreationRegression.exe > native-editor-creation-test.txt 2>&1
set "creation_result=%errorlevel%"
type native-editor-creation-test.txt
exit /b %creation_result%
