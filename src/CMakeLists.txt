cmake_minimum_required(VERSION 3.20)
project(RawMetal LANGUAGES C CXX)
enable_language(RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(RawMetalAssetPacker tools/pack_assets.cpp)
target_compile_definitions(RawMetalAssetPacker PRIVATE NOMINMAX)
target_link_libraries(RawMetalAssetPacker PRIVATE cabinet bcrypt)
set_target_properties(RawMetalAssetPacker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
add_executable(RawMetalClutterImport EXCLUDE_FROM_ALL tools/import_clutter.cpp ThirdParty/ufbx/ufbx.c)
add_executable(RawMetalCreatureBake EXCLUDE_FROM_ALL tools/bake_creature.cpp ThirdParty/ufbx/ufbx.c)
add_executable(RawMetalHazmatImport EXCLUDE_FROM_ALL tools/import_hazmat.cpp ThirdParty/ufbx/ufbx.c)
set_target_properties(RawMetalHazmatImport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
set_target_properties(RawMetalCreatureBake PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
set_target_properties(RawMetalClutterImport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
file(GLOB_RECURSE RAWMETAL_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
get_filename_component(RAWMETAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(RAWMETAL_PACKED_RC "${CMAKE_BINARY_DIR}/packed/assets-packed.rc")
add_custom_command(OUTPUT "${RAWMETAL_PACKED_RC}"
    COMMAND $<TARGET_FILE:RawMetalAssetPacker> "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/packed" "${CMAKE_BINARY_DIR}/packed/unused-bundle.bin"
    DEPENDS RawMetalAssetPacker assets.rc ${RAWMETAL_ASSETS}
    COMMENT "Losslessly packing embedded assets" VERBATIM)
add_executable(RawMetal WIN32
    main.cpp
    app.rc
    "${RAWMETAL_PACKED_RC}"
    game/Game.cpp
    game/Settings.cpp
    game/Saves.cpp
    game/Inventory.cpp
    game/Movement.cpp
    game/GantryTest.cpp
    game/Lift.cpp
    game/Reactor.cpp
    game/Console.cpp
    game/Streaming.cpp
    game/Clutter.cpp
    game/Ragdoll.cpp
    game/AI.cpp
    audio/AudioEngine.cpp
    audio/DecodeMusic.cpp
    world/World.cpp
    renderer/SoftwareRenderer.cpp
    renderer/NormalMapping.cpp
    renderer/Mesh.cpp
    renderer/Scene3D.cpp
    renderer/Performance.cpp
    renderer/GpuRenderer.cpp
    ThirdParty/ufbx/ufbx.c
    platform/Win32Window.cpp)
# One published game executable for every build configuration.
get_filename_component(RAWMETAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set_target_properties(RawMetal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${RAWMETAL_ROOT}/$<0:>"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>")
target_include_directories(RawMetal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(RawMetal PRIVATE NOMINMAX UNICODE _UNICODE)
file(GLOB RAWMETAL_VULKAN_SDKS "C:/VulkanSDK/*")
list(APPEND CMAKE_PREFIX_PATH ${RAWMETAL_VULKAN_SDKS})
find_package(Vulkan REQUIRED COMPONENTS glslc)
set(RAWMETAL_SHADERS "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${RAWMETAL_SHADERS}")
foreach(STAGE vert frag)
  add_custom_command(OUTPUT "${RAWMETAL_SHADERS}/scene.${STAGE}.inc"
    COMMAND Vulkan::glslc --target-env=vulkan1.0 -O -mfmt=c "${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/scene.${STAGE}" -o "${RAWMETAL_SHADERS}/scene.${STAGE}.inc"
    DEPENDS "renderer/shaders/scene.${STAGE}" VERBATIM)
  target_sources(RawMetal PRIVATE "${RAWMETAL_SHADERS}/scene.${STAGE}.inc")
endforeach()
target_include_directories(RawMetal PRIVATE "${RAWMETAL_SHADERS}")
target_link_libraries(RawMetal PRIVATE user32 gdi32 winmm cabinet bcrypt Vulkan::Vulkan)
if(MSVC)
  target_link_libraries(RawMetal PRIVATE delayimp)
  target_link_options(RawMetal PRIVATE /DELAYLOAD:vulkan-1.dll)
endif()
add_custom_command(TARGET RawMetal POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-DRAWMetalExecutable=$<TARGET_FILE:RawMetal>" -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_size.cmake"
    VERBATIM)
if(MSVC)
  target_compile_options(RawMetal PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(RawMetal PRIVATE -Wall -Wextra -Wpedantic)
endif()

