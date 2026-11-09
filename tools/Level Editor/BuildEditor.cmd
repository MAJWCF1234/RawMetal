@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist ".build\native-ui" mkdir ".build\native-ui"
cl /nologo /utf-8 /std:c++20 /O2 /EHsc editor\NativeEditor.cpp /Fo:.build\native-ui\NativeEditor.obj /Fe:.build\native-ui\RawMetalEditor.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -ExecutionPolicy Bypass -File editor\PublishEditor.ps1
exit /b %errorlevel%
