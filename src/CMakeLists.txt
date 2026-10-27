cmake_minimum_required(VERSION 3.20)
project(RawMetal LANGUAGES C CXX)
enable_language(RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Pinned, vendored lossless texture codec. Only the decoder enters the game;
# the encoder remains a build-time tool. No new runtime DLL is required.
set(BUILD_SHARED_LIBS OFF)
foreach(OPTION ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS WEBP_JS FUZZTEST)
    set(WEBP_BUILD_${OPTION} OFF CACHE BOOL "Unused WebP tool" FORCE)
endforeach()
set(WEBP_NEAR_LOSSLESS OFF CACHE BOOL "Never permit lossy texture preprocessing" FORCE)
add_subdirectory(ThirdParty/libwebp EXCLUDE_FROM_ALL)
add_executable(RawMetalAssetPacker tools/pack_assets.cpp)
add_library(RawMetalNeuralTextures STATIC core/NeuralTexture.cpp)
target_link_libraries(RawMetalNeuralTextures PUBLIC webpdecoder)
target_link_libraries(RawMetalAssetPacker PRIVATE RawMetalNeuralTextures)
add_executable(RawMetalNeuralTextureTest EXCLUDE_FROM_ALL tools/test_neural_texture.cpp)
target_link_libraries(RawMetalNeuralTextureTest PRIVATE RawMetalNeuralTextures webp)
target_compile_definitions(RawMetalNeuralTextureTest PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_definitions(RawMetalAssetPacker PRIVATE NOMINMAX)
target_link_libraries(RawMetalAssetPacker PRIVATE cabinet bcrypt webp)
set_target_properties(RawMetalAssetPacker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
add_executable(RawMetalAnimationCodecTest EXCLUDE_FROM_ALL tools/test_animation_predictor.cpp)
target_compile_definitions(RawMetalAnimationCodecTest PRIVATE RAWMETAL_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(RawMetalAnimationCodecTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
add_executable(RawMetalClutterImport EXCLUDE_FROM_ALL tools/import_clutter.cpp ThirdParty/ufbx/ufbx.c)
add_executable(RawMetalCreatureBake EXCLUDE_FROM_ALL tools/bake_creature.cpp ThirdParty/ufbx/ufbx.c)
add_executable(RawMetalAuthoredCreatureBake EXCLUDE_FROM_ALL tools/bake_authored_creature.cpp ThirdParty/ufbx/ufbx.c)
set_target_properties(RawMetalAuthoredCreatureBake PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
add_executable(RawMetalHazmatImport EXCLUDE_FROM_ALL tools/import_hazmat.cpp ThirdParty/ufbx/ufbx.c)
set_target_properties(RawMetalHazmatImport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
set_target_properties(RawMetalCreatureBake PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
set_target_properties(RawMetalClutterImport PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/$<0:>")
# The packer's inputs and outputs come from the manifest itself: every file
# assets.rc names (wherever it lives, junctioned library folders included) and
# the .rmz it becomes. A glob cannot see through the worktrees' junctions, so
# an edited library asset was never repacked, and a deleted .rmz was never
# rebuilt; both now rerun the packer.
set(RAWMETAL_ASSETS)
set(RAWMETAL_RMZ)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/assets.rc" RAWMETAL_MANIFEST_LINES REGEX "^[0-9]+[ 	]+[A-Za-z]+[ 	]+\"")
foreach(RAWMETAL_LINE IN LISTS RAWMETAL_MANIFEST_LINES)
    string(REGEX MATCH "^([0-9]+)[ 	]+[A-Za-z]+[ 	]+\"([^\"]+)\"" RAWMETAL_MATCH "${RAWMETAL_LINE}")
    list(APPEND RAWMETAL_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_MATCH_2}")
    list(APPEND RAWMETAL_RMZ "${CMAKE_BINARY_DIR}/packed/${CMAKE_MATCH_1}.rmz")
endforeach()
get_filename_component(RAWMETAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/packed")
# Mirror the manifest into the build tree. configure_file tracks assets.rc by
# content and refreshes this dependency when Git adds/removes resource IDs,
# preventing a stale generated RC from surviving an incremental checkout.
set(RAWMETAL_ASSET_MANIFEST_DEP "${CMAKE_BINARY_DIR}/packed/assets-manifest.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets.rc" "${RAWMETAL_ASSET_MANIFEST_DEP}" COPYONLY)
set(RAWMETAL_PACKED_RC "${CMAKE_BINARY_DIR}/packed/assets-packed.rc")
add_custom_command(OUTPUT "${RAWMETAL_PACKED_RC}" ${RAWMETAL_RMZ}
    COMMAND $<TARGET_FILE:RawMetalAssetPacker> "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/packed" "${CMAKE_BINARY_DIR}/packed/unused-bundle.bin"
    DEPENDS RawMetalAssetPacker "${RAWMETAL_ASSET_MANIFEST_DEP}" ${RAWMETAL_ASSETS}
    COMMENT "Losslessly packing embedded assets" VERBATIM)
add_executable(RawMetal WIN32
    main.cpp
    vr/VrRuntime.cpp
    app.rc
    "${RAWMETAL_PACKED_RC}"
    game/Game.cpp
    game/Script.cpp
    game/Settings.cpp
    game/Saves.cpp
    game/Inventory.cpp
    game/Movement.cpp
    game/GantryTest.cpp
    game/EngineRegressionTest.cpp
    game/Lift.cpp
    game/Reactor.cpp
    game/Console.cpp
    game/Streaming.cpp
    game/Mechanisms.cpp
    game/ActorTimeline.cpp
    game/ai/Hearing.cpp
    game/ai/Nodes.cpp
    game/ai/NavGraph.cpp
    game/ai/enemy/EnemyAI.cpp
    game/ai/enemy/Schedule.cpp
    game/ai/enemy/ScheduleTest.cpp
    game/ai/friendly/FriendlyAI.cpp
    game/ai/friendly/Voices.cpp
    game/ai/friendly/Streaming.cpp
    game/ai/friendly/CivilianEscort.cpp
    game/ai/friendly/Schedule.cpp
    game/ai/friendly/ScheduleTest.cpp
    game/ai/friendly/Mind.cpp
    game/ai/guard/GuardCtx.cpp
    game/ai/guard/Mind.cpp
    game/ai/guard/Combat.cpp
    game/ai/guard/Tactics.cpp
    game/ai/guard/Targets.cpp
    game/ai/guard/Squads.cpp
    game/ai/guard/Danger.cpp
    game/ai/guard/Schedule.cpp
    game/ai/guard/Support.cpp
    game/ai/guard/Casualties.cpp
    game/ai/guard/Plans.cpp
    game/ai/guard/Conditions.cpp
    game/ai/guard/tests/GuardTest.cpp
    game/ai/guard/tests/CombatTest.cpp
    game/ai/guard/tests/TacticsTest.cpp
    game/ai/guard/tests/PolicyTest.cpp
    game/ai/guard/tests/DefenseTest.cpp
    game/ai/guard/tests/PursuitTest.cpp
    game/ai/guard/tests/ActionTest.cpp
    game/ai/guard/tests/RoleTest.cpp
    game/ai/guard/tests/DangerTest.cpp
    game/ai/guard/tests/ScheduleTest.cpp
    game/ai/guard/tests/PatientTest.cpp
    game/ai/guard/tests/CoverTest.cpp
    game/ai/guard/tests/TableTest.cpp
    game/ai/guard/tests/NavGraphTest.cpp
    game/ai/guard/tests/DecisionTest.cpp
    game/ai/guard/tests/CautionTest.cpp
    game/ai/engineer/Drones.cpp
    game/ai/engineer/Support.cpp
    game/ai/engineer/DronesTest.cpp
    game/ai/grenades/TacticalGrenades.cpp
    game/ai/grenades/TacticalGrenadeTest.cpp
    game/ai/debug/TacticsInspection.cpp
    game/ai/debug/PolicyInspection.cpp
    game/ai/debug/DangerInspection.cpp
    game/ai/debug/EngineerInspection.cpp
    game/ai/debug/KillSquadInspection.cpp
    game/ai/debug/SquadSupportInspection.cpp
    game/Hitscan.cpp
    game/Radar.cpp
    world/Ballistics.cpp
    game/CampaignMapTest.cpp
    game/WorldIsolationTest.cpp
    game/ExclusionTest.cpp
    game/ExclusionAuthoringTest.cpp
    game/PrologueTest.cpp
    game/MapAudit.cpp
    game/Stealth.cpp
    game/Crates.cpp
    game/ServiceMapTest.cpp
    game/Clutter.cpp
    game/Ragdoll.cpp
    audio/AudioEngine.cpp
    audio/DecodeMusic.cpp
    world/World.cpp
    world/CampaignMaps.cpp
    world/CampaignChapters.cpp
    world/CampaignChapterPower.cpp
    world/CampaignChapterMachineHall.cpp
    world/CampaignChapterWater.cpp
    world/CampaignChapterMaintenance.cpp
    world/CampaignChapterPerimeterA.cpp
    world/CampaignChapterPerimeterB.cpp
    world/CampaignChapterResearchFront.cpp
    world/CampaignChapterMedicalFront.cpp
    world/CampaignChapterMedicalWard.cpp
    world/CampaignChapterAnalysis.cpp
    world/CampaignChapterEnergyTestLabs.cpp
    world/CampaignChapterObservationRing.cpp
    world/CampaignChapterContainmentA.cpp
    world/CampaignChapterContainmentB.cpp
    world/CampaignChapterDeepResearch.cpp
    world/CampaignChapterMinehead.cpp
    world/CampaignChapterUpperMineA.cpp
    world/CampaignChapterUpperMineB.cpp
    world/CampaignChapterLowerMine.cpp
    world/CampaignChapterDeepBore.cpp
    world/CampaignChapterWildsA.cpp
    world/CampaignChapterWildsB.cpp
    world/CampaignChapterScrap.cpp
    world/CampaignChapterHavenA.cpp
    world/CampaignChapterHavenB.cpp
    world/CampaignChapterFinaleA.cpp
    world/CampaignChapterFinaleB.cpp
    world/CampaignChapterFinaleC.cpp
    world/CustomCampaign.cpp
    renderer/SoftwareRenderer.cpp
    renderer/SurfaceSetTest.cpp
    renderer/NormalMapping.cpp
    renderer/Mesh.cpp
    renderer/Scene3D.cpp
    renderer/Impostors.cpp
    renderer/Performance.cpp
    renderer/GpuRenderer.cpp
    ThirdParty/ufbx/ufbx.c
    platform/Win32Window.cpp)
# One published game executable for every build configuration.
get_filename_component(RAWMETAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(RAWMETAL_RUNTIME_DIRECTORY "${RAWMETAL_ROOT}" CACHE PATH "Game output directory (override for testing while the game is running)")
set_target_properties(RawMetal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${RAWMETAL_RUNTIME_DIRECTORY}/$<0:>"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/symbols/$<CONFIG>")
target_include_directories(RawMetal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(RawMetal PRIVATE NOMINMAX UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
file(GLOB RAWMETAL_VULKAN_SDKS "C:/VulkanSDK/*")
list(APPEND CMAKE_PREFIX_PATH ${RAWMETAL_VULKAN_SDKS})
find_package(Vulkan REQUIRED COMPONENTS glslc)
set(RAWMETAL_SHADERS "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${RAWMETAL_SHADERS}")
foreach(SHADER scene.vert scene.frag composite.vert composite.frag post.frag occlusion.frag shadow.frag lampshadow.vert lampshadow.frag taa.frag sunshadow.vert sunshadow.frag exposure.frag)
  add_custom_command(OUTPUT "${RAWMETAL_SHADERS}/${SHADER}.inc"
    COMMAND Vulkan::glslc --target-env=vulkan1.0 -O -mfmt=c "${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/${SHADER}" -o "${RAWMETAL_SHADERS}/${SHADER}.inc"
    DEPENDS "renderer/shaders/${SHADER}" VERBATIM)
  target_sources(RawMetal PRIVATE "${RAWMETAL_SHADERS}/${SHADER}.inc")
endforeach()
add_custom_command(OUTPUT "${RAWMETAL_SHADERS}/actor.vert.inc"
  COMMAND Vulkan::glslc --target-env=vulkan1.0 -O -DRAWMETAL_ACTOR_INSTANCES -mfmt=c "${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/scene.vert" -o "${RAWMETAL_SHADERS}/actor.vert.inc"
  DEPENDS "renderer/shaders/scene.vert" VERBATIM)
target_sources(RawMetal PRIVATE "${RAWMETAL_SHADERS}/actor.vert.inc")
target_include_directories(RawMetal PRIVATE "${RAWMETAL_SHADERS}")
target_link_libraries(RawMetal PRIVATE user32 gdi32 winmm cabinet bcrypt advapi32 comctl32 Vulkan::Vulkan webpdecoder)
target_link_libraries(RawMetal PRIVATE RawMetalNeuralTextures)

if(MSVC)
  target_link_libraries(RawMetal PRIVATE delayimp)
  target_link_options(RawMetal PRIVATE /DELAYLOAD:vulkan-1.dll "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
  set_property(TARGET RawMetal APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
endif()
add_custom_command(TARGET RawMetal POST_BUILD
    COMMAND $<TARGET_FILE:RawMetalAssetPacker> --verify-exe "${CMAKE_CURRENT_SOURCE_DIR}" "$<TARGET_FILE:RawMetal>"
    COMMAND ${CMAKE_COMMAND} "-DRAWMetalExecutable=$<TARGET_FILE:RawMetal>" -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_size.cmake"
    COMMENT "Verifying embedded assets are lossless and release-size safe"
    VERBATIM)
if(MSVC)
  target_compile_options(RawMetal PRIVATE /W4 /permissive- /EHsc)
  # Fold unused/identical native code without changing assets or FP semantics.
  # Cold import sources below retain their size-oriented /O1 optimizations.
  target_compile_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/Gy>" "$<$<CONFIG:Release>:/Gw>")
  # Spend native-code headroom on inlining, retaining precise floating point
  # and the existing baseline ISA. The executable size verifier still applies.
  target_compile_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/Ob3>")
  target_link_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>" "/MAP:${CMAKE_BINARY_DIR}/../RawMetal.map")
  # Optional native PGO: train the instrumented executable on representative
  # gameplay, then relink the same sources with USE. No ISA or FP changes.
  set(RAWMETAL_PGO_PHASE "OFF" CACHE STRING "Native PGO phase: OFF, GENERATE or USE")
  set_property(CACHE RAWMETAL_PGO_PHASE PROPERTY STRINGS OFF GENERATE USE)
  set(RAWMETAL_PGO_DATABASE "${CMAKE_BINARY_DIR}/RawMetal.pgd" CACHE FILEPATH "Native PGO training database")
  option(RAWMETAL_PGO_AGGRESSIVE "Compete speed-first PGO against ordinary PGO (size verifier still applies)" OFF)
  if(RAWMETAL_PGO_PHASE STREQUAL "GENERATE")
    target_link_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/FASTGENPROFILE:PGD=${RAWMETAL_PGO_DATABASE}>")
  elseif(RAWMETAL_PGO_PHASE STREQUAL "USE")
    if(NOT EXISTS "${RAWMETAL_PGO_DATABASE}")
      message(FATAL_ERROR "PGO database is missing; build GENERATE and train that executable first")
    endif()
    if(RAWMETAL_PGO_AGGRESSIVE)
      target_link_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/USEPROFILE:AGGRESSIVE>" "$<$<CONFIG:Release>:/USEPROFILE:PGD=${RAWMETAL_PGO_DATABASE}>")
    else()
      target_link_options(RawMetal PRIVATE "$<$<CONFIG:Release>:/USEPROFILE:PGD=${RAWMETAL_PGO_DATABASE}>")
    endif()
    set_property(TARGET RawMetal APPEND PROPERTY LINK_DEPENDS "${RAWMETAL_PGO_DATABASE}")
  elseif(NOT RAWMETAL_PGO_PHASE STREQUAL "OFF")
    message(FATAL_ERROR "RAWMETAL_PGO_PHASE must be OFF, GENERATE or USE")
  endif()
  set_source_files_properties(vr/VrRuntime.cpp ThirdParty/ufbx/ufbx.c PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:/O1>")
else()
  target_compile_options(RawMetal PRIVATE -Wall -Wextra -Wpedantic)
endif()

