cmake_minimum_required(VERSION 3.24)
project(VekksAtlas VERSION 1.0.0 LANGUAGES CXX RC)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Nexus API 6 uses the ImGui 1.80 context bundled here.
set(CMM_VENDOR_DIR "${CMAKE_SOURCE_DIR}/external")
set(imgui_SOURCE_DIR "${CMM_VENDOR_DIR}/imgui")

add_library(VekksAtlas SHARED
  src/Addon.cpp
  src/AtlasLocalization.cpp
  src/AtlasColors.cpp
  src/AtlasShortcut.cpp
  src/SettingsLink.cpp
  src/MarkerLink.cpp
  src/AtlasClipboard.cpp
  src/AdvancedPlacement.cpp
  src/CompetitiveCoordinates.cpp
  src/WorldBeacons.cpp
  src/WorldReference.cpp
  src/Version.rc
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp)

target_include_directories(VekksAtlas PRIVATE
  src
  ${CMM_VENDOR_DIR}
  ${imgui_SOURCE_DIR})
target_compile_definitions(VekksAtlas PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(VekksAtlas PRIVATE user32 bcrypt)
set_target_properties(VekksAtlas PROPERTIES
  OUTPUT_NAME "VekksAtlas"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")

if(MSVC)
  target_compile_options(VekksAtlas PRIVATE /W4 /permissive- /utf-8 /Zi)
  target_link_options(VekksAtlas PRIVATE /DEBUG:FULL /INCREMENTAL:NO "/PDBALTPATH:%_PDB%" "/MAP:${CMAKE_SOURCE_DIR}/dist/$<CONFIG>/VekksAtlas.map")
else()
  target_compile_options(VekksAtlas PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(MarkerLinkTests
    tests/MarkerLinkTests.cpp
    src/AtlasShortcut.cpp
    src/MarkerLink.cpp
    src/AtlasLocalization.cpp)
  target_include_directories(MarkerLinkTests PRIVATE src external)
  target_compile_definitions(MarkerLinkTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(MarkerLinkTests PRIVATE bcrypt user32)
  if(MSVC)
    target_compile_options(MarkerLinkTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME MarkerLinkTests COMMAND MarkerLinkTests)

  add_executable(AddonDefinitionTests tests/AddonDefinitionTests.cpp)
  target_include_directories(AddonDefinitionTests PRIVATE external)
  target_compile_definitions(AddonDefinitionTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_dependencies(AddonDefinitionTests VekksAtlas)
  add_test(NAME AddonDefinitionTests COMMAND AddonDefinitionTests $<TARGET_FILE:VekksAtlas>)

  add_executable(RuntimeLogicTests
    tests/RuntimeLogicTests.cpp
    src/AdvancedPlacement.cpp
    src/AtlasColors.cpp
    src/AtlasLocalization.cpp
    src/AtlasShortcut.cpp
    src/CompetitiveCoordinates.cpp
    src/SettingsLink.cpp
    src/WorldBeacons.cpp
    src/WorldReference.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
  target_include_directories(RuntimeLogicTests PRIVATE src external ${imgui_SOURCE_DIR})
  target_compile_definitions(RuntimeLogicTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(RuntimeLogicTests PRIVATE bcrypt user32)
  if(MSVC)
    target_compile_options(RuntimeLogicTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME RuntimeLogicTests COMMAND RuntimeLogicTests)
endif()
