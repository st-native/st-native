@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem Windows x64: fetch the engine at its pinned revision with our patches (spike\m1a\deps.lock.json, patches\), then
rem build the recompiler (ntr-recomp), the runtime and our programs (st_native, ntr_run) with the engine's "release"
rem preset (Ninja, clang-cl, lld-link). Needs no ROM.
rem   tools\build\build_program.cmd [--tests]     (--tests: also build everything and run the engine's unit tests)
setlocal
set ROOT=%~dp0..\..
set ENGINE=%ROOT%\.tools\upstream\ffcc
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
if not exist "%VSDIR%" set VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || (echo VS 2022 C++ build tools not found & exit /b 1)
rem clang-cl / lld-link from LLVM; Ninja as bundled with Visual Studio
set PATH=C:\Program Files\LLVM\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
set PY=%ROOT%\.venv\Scripts\python.exe
if not exist "%PY%" set PY=python

"%PY%" -I "%ROOT%\spike\m1a\fetch_engine.py" --dest "%ENGINE%" || (echo fetch FAILED & exit /b 2)
cd /d "%ENGINE%" || exit /b 2
cmake --preset release "-DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE=%ROOT%\app\st_targets.cmake" || (echo configure FAILED & exit /b 3)
cmake --build --preset release --target ntr-recomp st_native ntr_run || (echo build FAILED & exit /b 4)
if /i "%~1"=="--tests" (
  cmake --build --preset release || (echo full build FAILED & exit /b 5)
  ctest --preset release || (echo unit tests FAILED & exit /b 6)
)
echo.
echo built: %ENGINE%\build\release\recompiler\ntr-recomp.exe
echo        %ENGINE%\build\release\st_native.exe
echo        %ENGINE%\build\release\ntr_run.exe
