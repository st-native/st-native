# SPDX-License-Identifier: GPL-3.0-or-later
# Our targets in the engine's build (included at the end of the engine's top-level CMakeLists.txt; see st_targets.cmake).

# ntr_run: the headless host of the M1a spike (synthetic cartridge, per-frame state hashes)
include(${ST_ROOT}/spike/m1a/host/m1a_targets.cmake)

# The game host is shared by the CLI and the desktop launcher.
add_library(st_game_host STATIC ${ST_ROOT}/app/st_native.cpp ${ST_ROOT}/app/audio/audio_devices.cpp
            ${ST_ROOT}/app/audio/mic_source.cpp ${ST_ROOT}/game/st/hooks/st_hooks.cpp
            ${ST_ROOT}/game/st/hooks/st_probe.cpp ${ST_ROOT}/game/st/hooks/name_keyboard.cpp)
target_include_directories(st_game_host PRIVATE ${ST_ROOT}/app ${ST_ROOT}/game/st/hooks)
target_link_libraries(st_game_host PUBLIC ntr_runtime ntr_common SDL3::SDL3-static)
add_executable(st_native ${ST_ROOT}/app/st_native_main.cpp)
target_link_libraries(st_native PRIVATE st_game_host)
if(NOT MSVC AND NOT APPLE)
  # (as for ntr_run: melonDS and ntr_runtime call each other, and GNU ld reads each static library once)
  target_link_libraries(st_native PRIVATE "$<LINK_GROUP:RESCAN,ntr_runtime,melonds_core,melonds_net>")
endif()

if(FFCC_BUILD_TESTS)
  add_executable(st_screen_layout_tests "${ST_ROOT}/tests/test_screen_layout.cpp")
  add_test(NAME st_screen_layout COMMAND st_screen_layout_tests)
  add_executable(st_cinematic_tests "${ST_ROOT}/tests/test_cinematic_projection.cpp"
                 "${CMAKE_SOURCE_DIR}/runtime/src/gx_match.cpp")
  target_include_directories(st_cinematic_tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime/src"
                             "${CMAKE_SOURCE_DIR}/runtime/include")
  add_test(NAME st_cinematic_projection COMMAND st_cinematic_tests)
  add_executable(st_graphics_inspection_tests "${ST_ROOT}/tests/test_graphics_inspection.cpp"
                 "${CMAKE_SOURCE_DIR}/runtime/src/gx_match.cpp")
  target_include_directories(st_graphics_inspection_tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime/src"
                             "${CMAKE_SOURCE_DIR}/runtime/include")
  add_test(NAME st_graphics_inspection COMMAND st_graphics_inspection_tests)
  add_executable(st_screen_context_tests "${ST_ROOT}/tests/test_screen_context.cpp")
  target_include_directories(st_screen_context_tests PRIVATE "${CMAKE_SOURCE_DIR}/runtime/include")
  add_test(NAME st_screen_context COMMAND st_screen_context_tests)
  add_executable(st_subtitle_composition_tests "${ST_ROOT}/tests/test_subtitle_composition.cpp")
  add_test(NAME st_subtitle_composition COMMAND st_subtitle_composition_tests)
  add_executable(st_launcher_preferences_tests "${ST_ROOT}/tests/test_launcher_preferences.cpp")
  add_test(NAME st_launcher_preferences COMMAND st_launcher_preferences_tests)
  add_executable(st_audio_tests "${ST_ROOT}/tests/test_audio.cpp" "${ST_ROOT}/app/audio/mic_source.cpp"
                 "${ST_ROOT}/app/audio/audio_devices.cpp")
  target_include_directories(st_audio_tests PRIVATE "${ST_ROOT}/app")
  target_link_libraries(st_audio_tests PRIVATE SDL3::SDL3-static)
  add_test(NAME st_audio COMMAND st_audio_tests)
endif()

# Source-checkout launcher for the Windows developer alpha. Reuse the engine's pinned RmlUi/GL3 backend.
if(WIN32)
  set(ST_LAUNCHER_RESOURCE "${ST_ROOT}/platform/windows/launcher.rc")
  add_executable(st_launcher WIN32
    "${ST_ROOT}/app/ui/launcher.cpp" "${ST_ROOT}/app/present/launcher_window.cpp"
    "${ST_ROOT}/platform/windows/launcher_worker.cpp" "${ST_LAUNCHER_RESOURCE}")
  set_source_files_properties("${ST_LAUNCHER_RESOURCE}" PROPERTIES
    OBJECT_DEPENDS "${ST_ROOT}/platform/windows/launcher.ico")
  target_include_directories(st_launcher PRIVATE "${ST_ROOT}/app" "${ST_ROOT}/app/setup")
  target_link_libraries(st_launcher PRIVATE st_game_host rmlui_backend melonds_core shell32)

  set(ST_LAUNCHER_ASSETS "$<TARGET_FILE_DIR:st_launcher>/launcher-assets")
  set(ST_LAUNCHER_FONTS "${CMAKE_SOURCE_DIR}/game/ui/res/fonts")
  add_custom_target(st_launcher_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ST_LAUNCHER_ASSETS}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ST_ROOT}/app/ui/launcher.rml" "${ST_ROOT}/app/ui/launcher.rcss"
      "${ST_ROOT}/app/ui/pause.rml"
      "${ST_LAUNCHER_FONTS}/Cinzel.ttf" "${ST_LAUNCHER_FONTS}/OFL-Cinzel.txt"
      "${ST_LAUNCHER_FONTS}/AlegreyaSans-Regular.ttf" "${ST_LAUNCHER_FONTS}/OFL-AlegreyaSans.txt"
      "${ST_LAUNCHER_ASSETS}"
    VERBATIM)
  add_dependencies(st_launcher st_launcher_assets)
endif()
