# SPDX-License-Identifier: GPL-3.0-or-later
# Our programs, added to the engine's own build (pinned revision: spike/m1a/deps.lock.json) without changing engine
# files. Configure the engine with
#   -DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE=<this file>
# CMake includes this file right after the engine's project(), before the engine sets its language standard, compile
# options and targets; our targets are therefore defined at the end of the engine's top-level CMakeLists.txt, with the
# same settings as the engine's own programs.
get_filename_component(ST_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(WIN32)
  enable_language(RC)
endif()
# (A deferred call expands its variables when it runs, where CMAKE_CURRENT_LIST_DIR is the engine's: hence ST_ROOT.)
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL include "${ST_ROOT}/app/st_program.cmake")
