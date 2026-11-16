@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem M1a V2 on Windows x64: build spike\m1a\fiber_conformance.cpp against the engine's fiber implementation and run it,
rem in a Debug and a Release build (optimisation changes which FP registers carry values across a switch).
setlocal
set ENGINE=%~1
if "%ENGINE%"=="" set ENGINE=%~dp0..\..\.tools\upstream\ffcc
set OUT=%~dp0results
if not exist "%OUT%" mkdir "%OUT%"
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%PATH%
set SRC="%~dp0fiber_conformance.cpp" "%ENGINE%\runtime\src\fiber.cpp"
set RC=0
for %%O in (Od O2) do (
  clang-cl /nologo /%%O /EHsc /std:c++17 /clang:-ffp-contract=off /I "%ENGINE%\runtime\src" %SRC% /Fe"%OUT%\fiber_conformance_%%O.exe" /Fo"%OUT%\\" > "%OUT%\v2_build_%%O.log" 2>&1 || (echo build %%O FAILED & set RC=2)
  "%OUT%\fiber_conformance_%%O.exe" > "%OUT%\v2_fiber_%%O.txt" 2>&1 || set RC=1
  findstr /c:"transcript hash" "%OUT%\v2_fiber_%%O.txt"
)
exit /b %RC%
