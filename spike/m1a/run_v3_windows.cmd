@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem M1a V3 on Windows x64: the engine's recompiler difftest (recompiled C vs Unicorn's ARM946, random instructions),
rem using ntr-recomp.exe from the V1 build. Needs the project's .venv (unicorn 2.1.4) and LLVM clang.
rem   spike\m1a\run_v3_windows.cmd [count per category] [seed]
setlocal
set ENGINE=%~dp0..\..\.tools\upstream\ffcc
set OUT=%~dp0results
set COUNT=%~1
if "%COUNT%"=="" set COUNT=300
set SEED=%~2
if "%SEED%"=="" set SEED=1
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%PATH%
"%~dp0..\..\.venv\Scripts\python.exe" "%ENGINE%\tests\difftest\difftest.py" --recomp "%ENGINE%\build\release\recompiler\ntr-recomp.exe" --count %COUNT% --seed %SEED% > "%OUT%\v3_difftest_seed%SEED%.txt" 2>&1
set RC=%ERRORLEVEL%
type "%OUT%\v3_difftest_seed%SEED%.txt"
exit /b %RC%
