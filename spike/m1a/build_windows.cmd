@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem M1a V1 on Windows x64: configure, build and unit-test the engine at the pinned revision (spike\m1a\deps.lock.json)
rem with the engine's own "release" preset (Ninja, clang-cl, lld-link). ROM-free.
rem   spike\m1a\build_windows.cmd [engine dir] [log dir]
setlocal
set ENGINE=%~1
if "%ENGINE%"=="" set ENGINE=%~dp0..\..\.tools\upstream\ffcc
set LOGS=%~2
if "%LOGS%"=="" set LOGS=%~dp0results
if not exist "%LOGS%" mkdir "%LOGS%"

set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem clang-cl / lld-link from LLVM, Ninja bundled with the Build Tools
set PATH=C:\Program Files\LLVM\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%

cd /d "%ENGINE%" || exit /b 1
echo == versions> "%LOGS%\v1_versions.txt"
clang-cl --version >> "%LOGS%\v1_versions.txt" 2>&1
cmake --version >> "%LOGS%\v1_versions.txt" 2>&1
ninja --version >> "%LOGS%\v1_versions.txt" 2>&1
cl 2>&1 | findstr /i "version" >> "%LOGS%\v1_versions.txt"

cmake --preset release > "%LOGS%\v1_configure.log" 2>&1 || (echo configure FAILED & exit /b 2)
cmake --build --preset release > "%LOGS%\v1_build.log" 2>&1 || (echo build FAILED & exit /b 3)
ctest --preset release > "%LOGS%\v1_ctest.log" 2>&1
set RC=%ERRORLEVEL%
echo configure ok, build ok, ctest exit %RC%
exit /b %RC%
