@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem M1a on Windows x64: run the synthetic cartridge through the recompilation base.
rem   1. synthetic ROM with the logo checksum (spike\m1a\zpoc_rom.py) and its recompiler config (make_zpoc_config.py)
rem   2. ntr-recomp --rom ... --config spike\m1a\zpoc_config  (C source)
rem   3. the C compiled into a game module with the engine's own module flags (clang-cl /O1, no C library)
rem   4. spike\m1a\host\ntr_run.cpp added to the engine's build (no engine file changed) and built
rem   5. N frames headless: twice with the module (determinism), once without (all on the interpreter)
rem   spike\m1a\run_zpoc_windows.cmd [frames]
setlocal
set ROOT=%~dp0..\..
set ENGINE=%ROOT%\.tools\upstream\ffcc
set OUT=%~dp0results\zpoc
set FRAMES=%~1
if "%FRAMES%"=="" set FRAMES=600
set PY=%ROOT%\.venv\Scripts\python.exe
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%

if exist "%OUT%" rmdir /s /q "%OUT%"
"%PY%" "%~dp0zpoc_rom.py" "%ROOT%\poc\out\sample.nds" "%OUT%\zpoc.nds" || exit /b 1
"%PY%" "%~dp0make_zpoc_config.py" --out "%~dp0zpoc_config" || exit /b 1
"%ENGINE%\build\release\recompiler\ntr-recomp.exe" --rom "%OUT%\zpoc.nds" --config "%~dp0zpoc_config" --out "%OUT%\gen" --report "%OUT%\recomp_report.txt" --allow-unknown-rom > "%OUT%\recomp.log" 2>&1 || (echo recompile FAILED & exit /b 2)

rem the game module: same flags as the engine's game/CMakeLists.txt (Windows)
pushd "%OUT%\gen"
clang-cl /nologo /w /GS- /Zl /O1 /DNTR_GAME_MODULE=1 /I "%ENGINE%\runtime\include" /I . *.c /LD /Fe"%OUT%\zpoc_game.dll" /link /NODEFAULTLIB /NOENTRY > "%OUT%\module_build.log" 2>&1 || (popd & echo module build FAILED & exit /b 3)
popd

rem the headless host, inside the engine's own build (app\st_targets.cmake adds it, with our other programs)
cmake -S "%ENGINE%" --preset release -DCMAKE_PROJECT_FFCCEOTRecomp_INCLUDE="%ROOT%\app\st_targets.cmake" > "%OUT%\host_configure.log" 2>&1 || (echo host configure FAILED & exit /b 4)
cmake --build "%ENGINE%\build\release" --target ntr_run > "%OUT%\host_build.log" 2>&1 || (echo host build FAILED & exit /b 4)

set RUN=%ENGINE%\build\release\ntr_run.exe
"%RUN%" "%OUT%\zpoc.nds" "%OUT%\zpoc_game.dll" %FRAMES% "%OUT%\hashes_recomp_1.txt"
"%RUN%" "%OUT%\zpoc.nds" "%OUT%\zpoc_game.dll" %FRAMES% "%OUT%\hashes_recomp_2.txt"
"%RUN%" "%OUT%\zpoc.nds" - %FRAMES% "%OUT%\hashes_interp.txt"
fc /b "%OUT%\hashes_recomp_1.txt" "%OUT%\hashes_recomp_2.txt" > nul && (echo recompiled run: identical frame hashes twice) || echo recompiled run: frame hashes DIFFER between two runs
