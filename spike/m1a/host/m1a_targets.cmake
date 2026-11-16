# SPDX-License-Identifier: GPL-3.0-or-later
# M1a: adds our generic headless host to the engine's own build without changing any engine file. Pass
#   -DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE=<this file>
# CMake includes it right after the engine's project(); ntr_runtime is defined later in the same configure run.
add_executable(ntr_run ${CMAKE_CURRENT_LIST_DIR}/ntr_run.cpp)
target_link_libraries(ntr_run PRIVATE ntr_runtime)
if(NOT MSVC AND NOT APPLE)
  # As the engine does for its own program (game/CMakeLists.txt): melonDS calls the Platform:: functions ntr_runtime
  # defines and ntr_runtime calls melonDS, and GNU ld reads each static library once, so rescan them as a group.
  # CMake has no RESCAN for Apple's linker, which needs none: ld64 resolves symbols across archives in any order.
  target_link_libraries(ntr_run PRIVATE "$<LINK_GROUP:RESCAN,ntr_runtime,melonds_core,melonds_net>")
endif()
