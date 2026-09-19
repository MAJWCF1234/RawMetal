@echo off
setlocal
where cmake >nul 2>nul || (
  echo CMake was not found. Install CMake and Visual Studio 2022 C++ tools.
  exit /b 1
)
cmake -S "%~dp0src" -B "%~dp0.build" -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build "%~dp0.build" --config Release || exit /b 1
echo Built: %~dp0RawMetal.exe
