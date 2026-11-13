# RawMetalLab: the feature lab. The engine's own sources (whatever the RawMetal
# target lists, so a file added there joins the lab too) plus src/lab/, built
# for fast iteration: no LTCG/PGO, no asset packing (assets load from the loose
# files assets.rc names), incremental linking, a precompiled standard library.
# EXCLUDE_FROM_ALL: the release build (Build.cmd, ALL_BUILD) never builds it and
# RawMetal's sources, flags and outputs are untouched by this file.
#   python tools/dev/lab.py build     (Ninja, its own build dir, no global lock)
get_target_property(RAWMETAL_LAB_ENGINE_SOURCES RawMetal SOURCES)
set(RAWMETAL_LAB_SOURCES)
foreach(SOURCE IN LISTS RAWMETAL_LAB_ENGINE_SOURCES)
  # Not the game's entry point, not the packed asset manifest or the override RCs.
  if(SOURCE STREQUAL "main.cpp" OR SOURCE MATCHES "\\.rc$")
    continue()
  endif()
  list(APPEND RAWMETAL_LAB_SOURCES "${SOURCE}")
endforeach()
file(GLOB RAWMETAL_LAB_OWN CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/lab/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lab/tubes/*.cpp")
add_executable(RawMetalLab EXCLUDE_FROM_ALL ${RAWMETAL_LAB_SOURCES} ${RAWMETAL_LAB_OWN} app.rc app.manifest)
target_include_directories(RawMetalLab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${RAWMETAL_SHADERS}")
target_compile_definitions(RawMetalLab PRIVATE NOMINMAX UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00
  RAWMETAL_LAB=1 "RAWMETAL_LAB_SOURCE=\"${CMAKE_CURRENT_SOURCE_DIR}\"" "RAWMETAL_LAB_ROOT=\"${RAWMETAL_ROOT}\""
  "RAWMETAL_LAB_SHADERS=\"${RAWMETAL_SHADERS}\"")
target_link_libraries(RawMetalLab PRIVATE user32 gdi32 winmm cabinet bcrypt advapi32 comctl32 ole32 windowscodecs
  Vulkan::Vulkan webpdecoder RawMetalNeuralTextures)
set_target_properties(RawMetalLab PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>"
  INTERPROCEDURAL_OPTIMIZATION FALSE)
# Optimisation for the lab: fast enough to run the software renderer and build
# maps in seconds, cheap to compile. RAWMETAL_LAB_OPT=/Od for a debugger build.
set(RAWMETAL_LAB_OPT "/O2;/Ob1" CACHE STRING "Lab compile optimisation flags")
if(MSVC)
  target_compile_options(RawMetalLab PRIVATE /EHsc /permissive- /W1 ${RAWMETAL_LAB_OPT} /Z7 /bigobj)
  target_link_options(RawMetalLab PRIVATE /STACK:8388608 /INCREMENTAL /DEBUG:FASTLINK)
  # The standard library once, for every C++ file (C files keep none).
  target_precompile_headers(RawMetalLab PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<vector>>" "$<$<COMPILE_LANGUAGE:CXX>:<string>>" "$<$<COMPILE_LANGUAGE:CXX>:<array>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<algorithm>>" "$<$<COMPILE_LANGUAGE:CXX>:<memory>>" "$<$<COMPILE_LANGUAGE:CXX>:<functional>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map>>" "$<$<COMPILE_LANGUAGE:CXX>:<map>>" "$<$<COMPILE_LANGUAGE:CXX>:<cmath>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cstdint>>" "$<$<COMPILE_LANGUAGE:CXX>:<cstring>>" "$<$<COMPILE_LANGUAGE:CXX>:<optional>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sstream>>" "$<$<COMPILE_LANGUAGE:CXX>:<fstream>>" "$<$<COMPILE_LANGUAGE:CXX>:<chrono>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<span>>" "$<$<COMPILE_LANGUAGE:CXX>:<string_view>>" "$<$<COMPILE_LANGUAGE:CXX>:<stdexcept>>")
  set_source_files_properties(ThirdParty/ufbx/ufbx.c TARGET_DIRECTORY RawMetalLab PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  # Hot files: the ones a worker is editing, compiled without optimisation so a
  # change rebuilds in seconds (Scene3D.cpp: ~70 s at /O2, ~5 s at /Od). They run
  # slower; correctness is the same. lab.py build --hot renderer/Scene3D.cpp
  set(RAWMETAL_LAB_HOT "" CACHE STRING "Sources (relative to src/) the lab compiles at /Od for fast rebuilds")
  foreach(HOT IN LISTS RAWMETAL_LAB_HOT)
    set_source_files_properties("${HOT}" TARGET_DIRECTORY RawMetalLab PROPERTIES SKIP_PRECOMPILE_HEADERS ON COMPILE_OPTIONS "/Od;/Ob0")
  endforeach()
endif()
