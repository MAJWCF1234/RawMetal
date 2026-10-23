@echo off
setlocal
where cmake >nul 2>nul || (
  echo CMake was not found. Install CMake and Visual Studio 2022 C++ tools.
  exit /b 1
)
rem Keep the native cache separate from legacy caches created on other drives.
rem Always emit the game to the repo root. A stale RAWMETAL_RUNTIME_DIRECTORY
rem override in the cache would otherwise leave an old RawMetal.exe here.
cmake -S "%~dp0src" -B "%~dp0.build\vs2022" -G "Visual Studio 17 2022" -A x64 "-DRAWMETAL_RUNTIME_DIRECTORY=%~dp0." || exit /b 1
cmake --build "%~dp0.build\vs2022" --config Release || exit /b 1
echo Built: %~dp0RawMetal.exe
