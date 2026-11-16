@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem M1a V3 diagnosis: run the engine's recompiler difftest one category at a time, so a crash names its category.
setlocal enabledelayedexpansion
set ENGINE=%~dp0..\..\.tools\upstream\ffcc
set OUT=%~dp0results
set COUNT=%~1
if "%COUNT%"=="" set COUNT=300
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%PATH%
for %%C in (arm_dp_imm arm_dp_regimm arm_dp_regreg arm_mul arm_mull arm_misc arm_ldr_str arm_ldr_unaligned arm_halfword arm_ldm_stm arm_swp thumb_alu thumb_mem thumb_ldr_pc) do (
  "%~dp0..\..\.venv\Scripts\python.exe" "%ENGINE%\tests\difftest\difftest.py" --recomp "%ENGINE%\build\release\recompiler\ntr-recomp.exe" --count %COUNT% --seed 1 --only %%C > "%OUT%\v3_cat_%%C.txt" 2>&1
  echo %%C exit !ERRORLEVEL!
)
