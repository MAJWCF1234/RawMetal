@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist ".build\native-ui" mkdir ".build\native-ui"
rem /Zi and /DEBUG keep a matching NativeEditor.pdb in .build\native-ui, so a crash log's RawMetalEditor.exe+offset can be read.
cl /nologo /utf-8 /std:c++20 /O2 /Zi /EHsc /bigobj editor\NativeEditor.cpp /Fo:.build\native-ui\NativeEditor.obj /Fd:.build\native-ui\NativeEditor.pdb /Fe:.build\native-ui\RawMetalEditor.exe /link /DEBUG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib
if errorlevel 1 exit /b %errorlevel%
if not exist ".build\sounds\celebrate.wav" python editor\PrepareSounds.py
powershell -NoProfile -ExecutionPolicy Bypass -File editor\PublishEditor.ps1
exit /b %errorlevel%
