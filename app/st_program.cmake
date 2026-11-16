# SPDX-License-Identifier: GPL-3.0-or-later
# Our targets in the engine's build (included at the end of the engine's top-level CMakeLists.txt; see st_targets.cmake).

# ntr_run: the headless host of the M1a spike (synthetic cartridge, per-frame state hashes)
include(${ST_ROOT}/spike/m1a/host/m1a_targets.cmake)

# st_native: the program (app/st_native.cpp) with the Spirit Tracks glue (game/st/hooks)
add_executable(st_native ${ST_ROOT}/app/st_native.cpp ${ST_ROOT}/game/st/hooks/st_hooks.cpp
               ${ST_ROOT}/game/st/hooks/st_probe.cpp)
target_include_directories(st_native PRIVATE ${ST_ROOT}/game/st/hooks)
target_link_libraries(st_native PRIVATE ntr_runtime ntr_common SDL3::SDL3-static)
if(NOT MSVC AND NOT APPLE)
  # (as for ntr_run: melonDS and ntr_runtime call each other, and GNU ld reads each static library once)
  target_link_libraries(st_native PRIVATE "$<LINK_GROUP:RESCAN,ntr_runtime,melonds_core,melonds_net>")
endif()
